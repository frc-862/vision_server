#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <unistd.h>
#include <chrono>

#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d.hpp>

#include "Pipeline.h"
#include "server.h"

using namespace std;
using namespace std::chrono;
using namespace cv;

bool running = true;
std::thread* vthread;

enum { camera = 3, segmented = 4 };

inline bool is_prefix(string const& prefix, string const& str) {
    return equal(prefix.begin(), prefix.end(), str.begin());
}

inline bool is_route(const string& route, const string& uri) {
    if (is_prefix(route, uri)) {
        if (route.size() == uri.size()) return true;

        char next_char = uri[route.size()];
        if (next_char == '/' || next_char == '?') return true;
    }

    return false;
}

#define ROUTE(name) \
  bool if_ ## name ## _route(const string& uri) { return is_route("/" #name, uri); } \
  bool if_ ## name ## _route(struct http_message *hm) { return if_ ## name ## _route(asString(hm->uri)); }

class VisionServer : public Server {
public:
    VisionServer(const std::string fname="vision_server.json") : Server(fname), 
      capture(0),
      counter(0), 
      save_images(false) 
      {
        vision_setup();
    }

    void http_get(struct mg_connection *nc, struct http_message *hm) override;

    void sent(struct mg_connection *nc) override;
    void send_image(struct mg_connection *nc, const cv::Mat& img);

    virtual void idle(mg_connection *connection) override;

    static void vision_thread(VisionServer *);
    void check_camera_data();
    
private:
    void send_mjpeg_header(mg_connection* nc, uintptr_t user_data);

    ROUTE(camera);
    ROUTE(segmented);
    ROUTE(contours);

    void vision_setup();

    void process_camera_data();
    void parse_target_info();
    void write_target_info();

    grip::Pipeline pipeline;
    cv::VideoCapture capture; 
    cv::Mat camera_frame;
    cv::Mat segmented_image;
		std::vector<std::vector<cv::Point>> contours;
    Json::Value contour_info;

    clock_t last_write;
    clock_t last_capture;
    size_t counter;
    bool save_images;
    std::string image_path;
    std::mutex mtx;
    time_point<system_clock> last_mjpg;
};

void VisionServer::send_mjpeg_header(mg_connection* nc, uintptr_t user_data) {
    // we want to stream, turn off the close flag
    nc->flags &= ~MG_F_SEND_AND_CLOSE;
    nc->user_data = (void*) user_data;
    mg_printf(nc,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: multipart/x-mixed-replace;boundary=b\r\n"
              "Cache-Control: no-store\r\n"
              "Pragma: no-cache\r\n"
              "Connection: close\r\n"
              "\r\n");
}

void VisionServer::sent(struct mg_connection *) {
}

void VisionServer::check_camera_data() {
    std::lock_guard<std::mutex> lck (mtx);

    //lastTargetImage = l_imgOut.clone();
            //last_capture = clock();
            //parse_target_info();
        //std::swap(segmented, l_imgOut);
    //parse_target_info
}

void VisionServer::process_camera_data() {
  capture >> camera_frame;
  if (camera_frame.data) {
    pipeline.setsource0(camera_frame);
    pipeline.Process();
    // todo 
    // save segmented image
    // track image time
    // save contours
  }
}

void VisionServer::vision_thread(VisionServer * vs) {
    while (running) {
        vs->process_camera_data();
        sleep(0);
    }
}

void VisionServer::parse_target_info() {
}

void VisionServer::write_target_info() {
    //return;
    if (save_images && (last_capture != last_write)) {
        last_write = last_capture;

        string fname = image_path + std::to_string(++counter); // + ".png";
        imwrite((fname + ".png").c_str(), segmented_image);
        ofstream of(fname + ".json");
        of << contour_info;
    }
}

void VisionServer::send_image(struct mg_connection *nc, const cv::Mat& img) {
    lock_guard<mutex> lg(mtx);
    vector<uchar> buf;
    imencode(".jpg", img, buf, { CV_IMWRITE_JPEG_QUALITY, 20 });

    mg_printf(nc,
              "--b\r\n"
              "Content-Type: image/jpeg\r\n"
              "Content-length: %d\r\n"
              "\r\n",(int) buf.size());
    mg_send(nc, &buf[0], buf.size());
}

void VisionServer::http_get(struct mg_connection *nc, struct http_message *hm) {
    string uri = asString(hm->uri);

    if (if_camera_route(uri)) {
        if (!camera_frame.empty()) {
            send_mjpeg_header(nc, camera);
        } else {
            send_http_error(nc, 404, "Not Found");
        }

    } else if (if_segmented_route(uri)) {
        if (!segmented_image.empty()) {
            send_mjpeg_header(nc, segmented);
        } else {
            send_http_error(nc, 404, "Not Found");
        }

    } else if (if_contours_route(uri)) {
        std::lock_guard<std::mutex> lck (mtx);
        send_http_json_response(nc, contour_info);

    } else {
        Server::http_get(nc, hm);
    }
}

static sig_atomic_t s_signal_received = 0;

static void signal_handler(int sig_num) {
    cout << "Shutting down" << endl;
    running = false;
    vthread->join();

    signal(sig_num, signal_handler);  // Reinstantiate signal handler
    s_signal_received = sig_num;
}

void VisionServer::idle(mg_connection *nc) {
    if (nc->user_data == nullptr) return;
    if (nc->send_mbuf.len > 0) return;

    time_point<system_clock> now = system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_mjpg).count();
    if (millis < 100) return;
    last_mjpg = now;

    switch (uintptr_t(nc->user_data)) {
    case camera:
        send_image(nc, camera_frame);
        break;

    case segmented:
        send_image(nc, segmented_image);
        break;
    }
}

void VisionServer::vision_setup() {
    cout << "vision setup" << endl;
    image_path = get_config("image_path", "");
    save_images = (image_path.size() > 0);
    if (save_images) {
        image_path += "/image-";
    }
}

void run_server()
{
    cout << "Create vision server" << endl;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    VisionServer server;
    cout << "vserve listening on port " << server.get_port() << endl;

    vthread = new std::thread(VisionServer::vision_thread, &server);

    while (s_signal_received == 0) {
        server.poll(100);
    }
}

int main(int argc, char** argv) {
    if (argc >= 2)
    {
        if (std::string(argv[1]) == "daemon") {
            cout << "Daemonizing vserver" << endl;
            int pid = fork();
            if (pid == -1) {
                cerr << "Unable to fork vserver" << endl;
                return -1;
            }

            if (pid == 0) {
                if (argc >= 3) {
                    chdir(argv[2]);
                } else {
                    chdir("/home/vision");
                }
                freopen("vision.log", "w", stdout);
                freopen("vision-err.log", "w", stderr);
                fclose(stdin);
            } else {
                cout << "vserver running in the background." << endl;
                return 0;
            }
        } else {
            cout << "Unknown argument >>>" << argv[1] << "<<<\n";
            return -2;
        }
    }

    run_server();
    return 0;
}

