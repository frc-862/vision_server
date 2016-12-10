#include <iostream>
#include <fstream>
#include "json/json.h"
#include "server.h"

using namespace std;

const struct mg_str Server::get_method = MG_STR("GET");
const struct mg_str Server::post_method = MG_STR("POST");
const struct mg_str Server::put_method = MG_STR("PUT");
const struct mg_str Server::delele_method = MG_STR("DELETE");

Server::Server(const Json::Value& config) {
    memset(&server_opts, 0, sizeof(server_opts));
    setup(config);
}

Server::Server(const string& fname) {
    memset(&server_opts, 0, sizeof(server_opts));
    try {
      ifstream stream(fname, ifstream::binary);
      stream >> server_config;
    } catch(Json::RuntimeError err) {
      cerr << "Error reading from " << fname << endl;
      cerr << err.what() << endl;
      server_config = Json::objectValue;
    }
    setup(server_config);
}

void Server::setup() {
    struct mg_connection *nc;
    mg_mgr_init(&mgr, this);

    nc = mg_bind(&mgr, port.c_str(), raw_event_handler);
    server_opts.document_root = root.c_str();
    mg_set_protocol_http_websocket(nc);
}

void Server::setup(const Json::Value& passed_config) {
    server_config = passed_config;

    port = get_config("port", "5801");
    root = get_config("document_root", "public");

    setup();
}

Server::~Server() {
    mg_mgr_free(&mgr);
}

void Server::event_handler(struct mg_connection *nc, int ev, void *ev_data) {
    switch (ev) {
    case MG_EV_HTTP_REQUEST:
        nc->flags |= MG_F_SEND_AND_CLOSE;
        http_request(nc, ev, (struct http_message*) ev_data);
        break;

    case MG_EV_SEND:
        sent(nc);
        break;

    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
        websocket_request(nc, (struct http_message*) ev_data);
        break;

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        websocket_create(nc);
        break;

    case MG_EV_WEBSOCKET_FRAME:
        websocket_frame(nc, (struct websocket_message*) ev_data);
        break;

    case MG_EV_CLOSE:
        if (is_websocket(nc)) {
            websocket_close(nc);
        }
        closed(nc);
        break;

    case MG_EV_POLL:
        idle(nc);
        break;
    }
}

void Server::raw_event_handler(struct mg_connection *nc, int ev, void *ev_data) {
    Server* me = (Server *) nc->mgr->user_data;
    me->event_handler(nc, ev, ev_data);
}

void Server::poll(int delay) {
    mg_mgr_poll(&mgr, delay);
}

void Server::http_request(struct mg_connection *nc, int, struct http_message *hm) {
    if (is_equal(&hm->method, &get_method)) {
        http_get(nc, hm);

    } else if (is_equal(&hm->method, &put_method)) {
        http_put(nc, hm);

    } else if (is_equal(&hm->method, &post_method)) {
        http_post(nc, hm);

    } else if (is_equal(&hm->method, &delele_method)) {
        http_delete(nc, hm);

    }
}

void Server::http_get(struct mg_connection *nc, struct http_message *hm) {
    auto handler = get_handler.find(uri_path(hm));
    if (handler != get_handler.end()) {
      handler->second(nc, hm);
    } else {
      std::cout << "GET: " << uri_path(hm) << std::endl;
      mg_serve_http(nc, hm, server_opts);
    }
}

void Server::http_put(struct mg_connection *nc, struct http_message *hm) {
    auto handler = put_handler.find(uri_path(hm));
    if (handler != put_handler.end()) {
      handler->second(nc, hm);
    } else {
      mg_serve_http(nc, hm, server_opts);
    }
}

void Server::http_post(struct mg_connection *nc, struct http_message *hm) {
    std::cout << "post: " << uri_path(hm) << std::endl;
    auto handler = post_handler.find(uri_path(hm));
    if (handler != post_handler.end()) {
      handler->second(nc, hm);
    } else {
      send_http_error(nc, 404, "Not Found");
    }
}

void Server::http_delete(struct mg_connection *nc, struct http_message *hm) {
    auto handler = delete_handler.find(uri_path(hm));
    if (handler != delete_handler.end()) {
      handler->second(nc, hm);
    } else {
      send_http_error(nc, 404, "Not Found");
    }
}

void Server::websocket_request(struct mg_connection *nc, struct http_message *) {
    send_http_error(nc, 404, "Not Found");
}

void Server::websocket_create(struct mg_connection *nc) {
    send_http_error(nc, 404, "Not Found");
}

void Server::websocket_frame(struct mg_connection *, struct websocket_message *) {
}

void Server::websocket_close(struct mg_connection *) {
}

void Server::send_http_response(struct mg_connection *nc, const string& body, int code, const string& reason) {
    mg_printf(nc, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n%s", code, 
        reason.c_str(), (int) body.size(), body.c_str());
}

void Server::send_http_error(struct mg_connection *nc, int code, const string& reason) {
    send_http_response(nc, "", code, reason);
}

// very opinionated response (404 on null values)
void Server::send_http_response(struct mg_connection *nc, const std::vector<uint8_t>& response, const string& type) {
    if ( response.size() == 0) {
        send_http_error(nc, 404, "Not Found");
    } else {
        mg_printf(nc, "HTTP/1.1 200 OK\r\n"
                          "Content-Length: %u\r\n"
                          "Content-Type: %s\r\n\r\n",
                  (unsigned) response.size(), type.c_str());
        mg_send(nc, &response[0], response.size());
    }
}

// very opinionated response (404 on null values)
void Server::send_http_json_response(struct mg_connection *nc, const Json::Value& response, int code, const string& reason) {
    if (response == Json::nullValue) {
        send_http_error(nc, 404, "Not Found");
    } else {
        //string buf = response.toStyledString();
        Json::StreamWriterBuilder wbuilder;
        //wbuilder["indentation"] = "\t";
        std::string buf = Json::writeString(wbuilder, response);
        //string buf = response.asString();

        mg_printf(nc, "HTTP/1.1 %d %s\r\n"
                          "Content-Length: %d\r\n"
                          "Content-Type: application/json\r\n\r\n"
                          "%s",
                  code, reason.c_str(),
                  (int) buf.size(), buf.c_str());
    }
}

Json::Value Server::json_body(struct http_message *hm) {
    Json::Reader reader;
    Json::Value value;
    if (reader.parse(asString(hm->body), value))
    {
        return value;
    }
    return Json::nullValue;
}

std::string Server::get_config(const std::string& key, const std::string& def)
{ 
  try {
    return get_config().get(key, def).asString(); 
  } catch (Json::RuntimeError& err) {
    return def;
  }
}
