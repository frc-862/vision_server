#ifndef SERVER_HPP__
#define SERVER_HPP__

#include <string>
#include <vector>
#include <functional>
#include "json/json.h"
#include "mongoose.h"

class Server {
public:
    Server(const Json::Value& config);
    Server(const std::string& fname);
    Server(const char* fname) : Server(std::string(fname)) {}

    virtual ~Server();

    virtual void idle(struct mg_connection*) {}
    virtual void poll(int sleep = 100);
    virtual void http_request(struct mg_connection *nc, int ev, struct http_message *hm);
    virtual void http_get(struct mg_connection *nc, struct http_message *hm);
    virtual void http_put(struct mg_connection *nc, struct http_message *hm);
    virtual void http_post(struct mg_connection *nc, struct http_message *hm);
    virtual void http_delete(struct mg_connection *nc, struct http_message *hm);
    virtual void sent(struct mg_connection *) {}
    virtual void closed(struct mg_connection *) {}

    virtual void websocket_request(struct mg_connection *nc, struct http_message *hm);
    virtual void websocket_create(struct mg_connection *nc);
    virtual void websocket_frame(struct mg_connection *nc, struct websocket_message *wm);
    virtual void websocket_close(struct mg_connection *nc);

    static std::string uri_path(const std::string& uri) {
      auto pos = uri.find("?");
      if (pos != std::string::npos) {
        return uri.substr(0, pos);
      } else {
        pos = uri.find("#");
        if (pos != std::string::npos)
          return uri.substr(0, pos);
      }
      return uri;
    }

    static std::string uri_path(http_message* hm) {
      return uri_path(asString(hm->uri));
    }

    static std::string uri_query(const std::string& uri);

    static bool is_equal(const struct mg_str *s1, const struct mg_str *s2) {
        return s1->len == s2->len && memcmp(s1->p, s2->p, s2->len) == 0;
    }

    static const struct mg_str get_method;
    static const struct mg_str post_method;
    static const struct mg_str put_method;
    static const struct mg_str delele_method;

    void send_http_response(struct mg_connection *nc, const std::vector<uint8_t>& body, const std::string& type="image/png");
    void send_http_response(struct mg_connection *nc, const std::string& body="", int code=200, const std::string& reason="OK");
    void send_http_error(struct mg_connection *nc, int code, const std::string& reason="");

    void send_http_json_response(struct mg_connection *nc, const Json::Value& response, int code=200, const std::string& reason="OK");

    Json::Value get_config() { return server_config; }
    Json::Value json_body(struct http_message *hm);
    std::string get_config(const std::string& key, const std::string& def); 
       // { return get_config().get(key, def).asString(); }
    
protected:
    struct mg_mgr mgr;
    struct mg_serve_http_opts server_opts;
    virtual void event_handler(struct mg_connection *nc, int ev, void *ev_data);

    virtual void setup();
    virtual void setup(const Json::Value& config);

    std::string port;
    std::string root;

    Json::Value server_config;

    bool is_websocket(const struct mg_connection *nc) {
        return nc->flags & MG_F_IS_WEBSOCKET;
    }

    using handler = std::function<void(mg_connection*, http_message*)>; 
    std::map<std::string, handler> get_handler;
    std::map<std::string, handler> put_handler;
    std::map<std::string, handler> post_handler;
    std::map<std::string, handler> delete_handler;

    void add_get_handler(const std::string& route, const handler& h) { get_handler[route] = h; }
    void add_put_handler(const std::string& route, const handler& h) { put_handler[route] = h; }
    void add_post_handler(const std::string& route, const handler& h) { post_handler[route] = h; }
    void add_delete_handler(const std::string& route, const handler& h) { delete_handler[route] = h; }
public:
    const std::string &get_port() const {
        return port;
    }

protected:
    static std::string asString(const mg_str& s) { return std::string(s.p, s.p + s.len); }
    static std::string asString(const mg_str* s) { return std::string(s->p, s->p + s->len); }
    
private:
    static void raw_event_handler(struct mg_connection *nc, int ev, void *ev_data);
};

#endif

