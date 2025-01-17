#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "eventuals/x509.h"

// Must be included after openssl includes.
#include "curl/curl.h"
#include "eventuals/event-loop.h"
#include "eventuals/scheduler.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {
namespace http {

////////////////////////////////////////////////////////////////////////

enum Method {
  GET,
  POST,
};

////////////////////////////////////////////////////////////////////////

// Used for application/x-www-form-urlencoded case.
// First string is key, second is value.
// TODO(folming): switch to RapidJSON.
using PostFields = std::vector<std::pair<std::string, std::string>>;
using Header = std::pair<std::string, std::string>;
using Headers = std::map<std::string, std::string>;

////////////////////////////////////////////////////////////////////////

class Request final {
 public:
  // Constructs a new http::Request "builder" with the default
  // undefined values.
  static auto Builder();

  const auto& uri() {
    return uri_;
  }

  const auto& method() {
    return method_;
  }

  const auto& headers() {
    return headers_;
  }

  const auto& body() {
    return body_;
  }

  const auto& timeout() {
    return timeout_;
  }

  const auto& fields() {
    return fields_;
  }

  const auto& verify_peer() {
    return verify_peer_;
  }

  const auto& certificate() {
    return certificate_;
  }

 private:
  friend class Client;

  template <bool, bool, bool, bool, bool, bool, bool>
  class _Builder;

  std::string uri_;
  Method method_;
  Headers headers_;
  std::string body_;
  std::chrono::nanoseconds timeout_;
  PostFields fields_;
  std::optional<bool> verify_peer_;
  std::optional<x509::Certificate> certificate_;
};

////////////////////////////////////////////////////////////////////////

template <
    bool has_uri_,
    bool has_method_,
    bool has_timeout_,
    bool has_fields_,
    bool has_verify_peer_,
    bool has_certificate_,
    bool has_headers_>
class Request::_Builder final : public builder::Builder {
 public:
  ~_Builder() override = default;

  auto uri(std::string&& uri) && {
    static_assert(!has_uri_, "Duplicate 'uri'");
    return Construct<_Builder>(
        uri_.Set(std::move(uri)),
        std::move(method_),
        std::move(timeout_),
        std::move(fields_),
        std::move(verify_peer_),
        std::move(certificate_),
        std::move(headers_));
  }

  auto method(Method method) && {
    static_assert(!has_method_, "Duplicate 'method'");
    return Construct<_Builder>(
        std::move(uri_),
        method_.Set(std::move(method)),
        std::move(timeout_),
        std::move(fields_),
        std::move(verify_peer_),
        std::move(certificate_),
        std::move(headers_));
  }

  auto timeout(std::chrono::nanoseconds&& timeout) && {
    static_assert(!has_timeout_, "Duplicate 'timeout'");
    return Construct<_Builder>(
        std::move(uri_),
        std::move(method_),
        timeout_.Set(std::move(timeout)),
        std::move(fields_),
        std::move(verify_peer_),
        std::move(certificate_),
        std::move(headers_));
  }

  auto fields(PostFields&& fields) && {
    static_assert(!has_fields_, "Duplicate 'fields'");
    return Construct<_Builder>(
        std::move(uri_),
        std::move(method_),
        std::move(timeout_),
        fields_.Set(std::move(fields)),
        std::move(verify_peer_),
        std::move(certificate_),
        std::move(headers_));
  }

  auto verify_peer(bool verify_peer) && {
    static_assert(!has_verify_peer_, "Duplicate 'verify_peer'");
    // TODO(benh): consider checking that the scheme is 'https'.
    return Construct<_Builder>(
        std::move(uri_),
        std::move(method_),
        std::move(timeout_),
        std::move(fields_),
        verify_peer_.Set(std::move(verify_peer)),
        std::move(certificate_),
        std::move(headers_));
  }

  // Specify the certificate to use when doing verification. Same
  // semantics as the following:
  //
  // $ curl --cacert /path/to/certificate ...
  //
  // TODO(benh): provide support for a "bundle" of certificates.
  auto certificate(x509::Certificate&& certificate) && {
    static_assert(!has_certificate_, "Duplicate 'certificate'");
    // TODO(benh): consider checking that the scheme is 'https'.
    return Construct<_Builder>(
        std::move(uri_),
        std::move(method_),
        std::move(timeout_),
        std::move(fields_),
        std::move(verify_peer_),
        certificate_.Set(std::move(certificate)),
        std::move(headers_));
  }

  auto header(std::string&& key, std::string&& value) && {
    // RFC 7230, section 3.2.2:
    // A sender MUST NOT generate multiple header fields with the same field
    // name in a message unless either the entire field value for that
    // header field is defined as a comma-separated list [i.e., #(values)]
    // or the header field is a well-known exception (as noted below).
    //
    // NOTE: If user tries to add an already existing header,
    // append the new one to the old one using comma.
    // Example:
    // Cookie: cookie1=value1, cookie2=value2
    auto iterator = headers_->find(key);
    if (iterator == headers_->end()) {
      // Header doesn't exist yet.
      headers_->emplace(std::move(key), std::move(value));
    } else {
      // Header already exists.
      iterator->second += ", ";
      iterator->second += std::move(value);
    }
    return Construct<_Builder>(
        std::move(uri_),
        std::move(method_),
        std::move(timeout_),
        std::move(fields_),
        std::move(verify_peer_),
        std::move(certificate_),
        headers_.Set(std::move(headers_).value()));
  }

  Request Build() && {
    static_assert(has_uri_, "Missing 'uri'");
    static_assert(has_method_, "Missing 'method'");

    Request request;
    request.uri_ = std::move(uri_).value();
    request.method_ = std::move(method_).value();

    if constexpr (has_timeout_) {
      request.timeout_ = std::move(timeout_).value();
    }

    if constexpr (has_fields_) {
      request.fields_ = std::move(fields_).value();
    }

    if constexpr (has_verify_peer_) {
      request.verify_peer_ = std::move(verify_peer_).value();
    }

    if constexpr (has_certificate_) {
      request.certificate_ = std::move(certificate_).value();
    }

    request.headers_ = std::move(headers_).value();

    return request;
  }

 private:
  friend class builder::Builder;
  friend class Request;

  _Builder() = default;

  _Builder(
      builder::Field<std::string, has_uri_> uri,
      builder::Field<Method, has_method_> method,
      builder::Field<std::chrono::nanoseconds, has_timeout_> timeout,
      builder::Field<PostFields, has_fields_> fields,
      builder::Field<bool, has_verify_peer_> verify_peer,
      builder::Field<x509::Certificate, has_certificate_> certificate,
      builder::RepeatedField<Headers, has_headers_> headers)
    : uri_(std::move(uri)),
      method_(std::move(method)),
      timeout_(std::move(timeout)),
      fields_(std::move(fields)),
      verify_peer_(std::move(verify_peer)),
      certificate_(std::move(certificate)),
      headers_(std::move(headers)) {}

  builder::Field<std::string, has_uri_> uri_;
  builder::Field<Method, has_method_> method_;
  builder::Field<std::chrono::nanoseconds, has_timeout_> timeout_;
  builder::Field<PostFields, has_fields_> fields_;
  builder::Field<bool, has_verify_peer_> verify_peer_;
  builder::Field<x509::Certificate, has_certificate_> certificate_;
  builder::RepeatedField<Headers, has_headers_> headers_ = Headers{};
};

////////////////////////////////////////////////////////////////////////

inline auto Request::Builder() {
  return Request::_Builder<false, false, false, false, false, false, false>();
}

////////////////////////////////////////////////////////////////////////

struct Response final {
  Response() = default;

  Response(const Response&) = default;
  Response(Response&&) = default;

  Response& operator=(const Response&) = default;
  Response& operator=(Response&&) = default;

  const auto& code() const {
    return code_;
  }

  const auto& headers() const {
    return headers_;
  }

  const auto& body() const {
    return body_;
  }

 private:
  friend struct _HTTP;

  Response(
      long code,
      Headers&& headers,
      std::string&& body)
    : code_(code),
      headers_(std::move(headers)),
      body_(std::move(body)) {}

  long code_;
  Headers headers_;
  std::string body_;
};

////////////////////////////////////////////////////////////////////////

class Client final {
 public:
  // Constructs a new http::Client "builder" with the default
  // undefined values.
  static auto Builder();

  Client() = default;

  auto Get(
      std::string&& uri,
      std::chrono::nanoseconds&& timeout = std::chrono::nanoseconds(0));

  auto Post(
      std::string&& uri,
      PostFields&& fields,
      std::chrono::nanoseconds&& timeout = std::chrono::nanoseconds(0));

  auto Do(Request&& request);

 private:
  template <bool, bool>
  class _Builder;

  std::optional<bool> verify_peer_;
  std::optional<x509::Certificate> certificate_;
};

////////////////////////////////////////////////////////////////////////

template <bool has_verify_peer_, bool has_certificate_>
class Client::_Builder final : public builder::Builder {
 public:
  ~_Builder() override = default;

  auto verify_peer(bool verify_peer) && {
    static_assert(!has_verify_peer_, "Duplicate 'verify_peer'");
    // TODO(benh): consider checking that the scheme is 'https'.
    return Construct<_Builder>(
        verify_peer_.Set(std::move(verify_peer)),
        std::move(certificate_));
  }

  // Specify the certificate to use when doing verification. Same
  // semantics as the following:
  //
  // $ curl --cacert /path/to/certificate ...
  //
  // TODO(benh): provide support for a "bundle" of certificates.
  auto certificate(x509::Certificate&& certificate) && {
    static_assert(!has_certificate_, "Duplicate 'certificate'");
    // TODO(benh): consider checking that the scheme is 'https'.
    return Construct<_Builder>(
        std::move(verify_peer_),
        certificate_.Set(std::move(certificate)));
  }

  Client Build() && {
    Client client;

    if constexpr (has_verify_peer_) {
      client.verify_peer_ = std::move(verify_peer_).value();
    }

    if constexpr (has_certificate_) {
      client.certificate_ = std::move(certificate_).value();
    }

    return client;
  }

 private:
  friend class builder::Builder;
  friend class Client;

  _Builder() {}

  _Builder(
      builder::Field<bool, has_verify_peer_> verify_peer,
      builder::Field<x509::Certificate, has_certificate_> certificate)
    : verify_peer_(std::move(verify_peer)),
      certificate_(std::move(certificate)) {}

  builder::Field<bool, has_verify_peer_> verify_peer_;
  builder::Field<x509::Certificate, has_certificate_> certificate_;
};

////////////////////////////////////////////////////////////////////////

inline auto Client::Builder() {
  return Client::_Builder<false, false>();
}

////////////////////////////////////////////////////////////////////////

// Our own eventual for using libcurl with the EventLoop.
//
// The general algorithm:
// 1. Create easy and multi handles. Set options for them.
//    Add easy handle to multi handle with curl_multi_add_handle.
//    TIMERFUNCTION is called to set a timer which will tell us when
//    to perform checks on libcurl handles.
//    SOCKETFUNCTION is called by using curl_multi_socket_action.
//    We can pass specific socket descriptor to work with that particular
//    socket or we can pass CURL_SOCKET_TIMEOUT to let libcurl call a function
//    for each socket that is currently in use.
// 2. Whenever SOCKETFUNCTION is called we check for events and set a poll
//    handle for the particular socket. This poll handle is created on heap
//    and that's why we put its pointer inside vector so that we can stop it
//    when we have to interrupt the transfer.
// 3. Whenever curl_multi_socket_action is called we can get an amount of
//    remaining running easy handles. If this value is 0 then we read info
//    from multi handle using check_multi_info lambda and clean everything up.
struct _HTTP final {
  template <typename K_>
  struct Continuation final {
    Continuation(K_ k, EventLoop& loop, Request&& request)
      : loop_(loop),
        request_(std::move(request)),
        fields_string_(nullptr, &curl_free),
        easy_(curl_easy_init(), &curl_easy_cleanup),
        multi_(curl_multi_init(), &curl_multi_cleanup),
        curl_headers_(nullptr, &curl_slist_free_all),
        context_(&loop, "HTTP (start/fail/stop)"),
        interrupt_context_(&loop_, "HTTP (interrupt)"),
        k_(std::move(k)) {}

    Continuation(Continuation&& that)
      : loop_(that.loop_),
        request_(std::move(that.request_)),
        fields_string_(std::move(that.fields_string_)),
        easy_(std::move(that.easy_)),
        multi_(std::move(that.multi_)),
        curl_headers_(std::move(that.curl_headers_)),
        context_(&that.loop_, "HTTP (start/fail/stop)"),
        interrupt_context_(&that.loop_, "HTTP (interrupt)"),
        k_(std::move(that.k_)) {
      CHECK(!that.started_ || !that.completed_) << "moving after starting";
      CHECK(!handler_);
    }

    ~Continuation() {
      CHECK(!started_ || closed_);
    }

    void Start() {
      CHECK(!started_ && !completed_);

      loop_.Submit(
          [this]() {
            if (!completed_) {
              started_ = true;

              CHECK(!error_);

              CHECK_NOTNULL(easy_);
              CHECK_NOTNULL(multi_);

              // If applicable, PEM encode any certificate now before we start
              // anything and can easily propagate an error.
              auto certificate = request_.certificate();
              if (certificate) {
                auto pem_certificate = pem::Encode(
                    x509::Certificate(*certificate));

                if (!pem_certificate) {
                  completed_ = true;

                  k_.Fail(std::runtime_error(
                      "Failed to PEM encode certificate"));

                  curl_easy_cleanup(easy_.get());
                  curl_multi_cleanup(multi_.get());

                  return; // Don't do anything else!
                } else {
                  curl_blob blob;
                  blob.data = pem_certificate->data();
                  blob.len = pem_certificate->size();
                  blob.flags = CURL_BLOB_COPY;
                  CHECK_EQ(
                      curl_easy_setopt(
                          easy_.get(),
                          CURLOPT_CAINFO_BLOB,
                          &blob),
                      CURLE_OK);
                }
              }

              CHECK_EQ(0, uv_timer_init(loop_, &timer_));
              uv_handle_set_data((uv_handle_t*) &timer_, this);

              // Called only one time to finish the transfer
              // and clean everything up.
              static auto check_multi_info = [](Continuation& continuation) {
                continuation.completed_ = true;

                // Stores the amount of remaining messages in multi handle.
                // Unused.
                int msgq = 0;
                CURLMsg* message = curl_multi_info_read(
                    continuation.multi_.get(),
                    &msgq);

                // Getting the response code and body.
                switch (message->msg) {
                  case CURLMSG_DONE:
                    if (message->data.result == CURLE_OK) {
                      curl_easy_getinfo(
                          continuation.easy_.get(),
                          CURLINFO_RESPONSE_CODE,
                          &continuation.code_);
                    } else {
                      continuation.error_ = message->data.result;
                    }
                    break;
                  default:
                    continuation.error_ = CURLE_ABORTED_BY_CALLBACK;
                    break;
                }

                // Stop transfer completely.
                CHECK_EQ(
                    curl_multi_remove_handle(
                        continuation.multi_.get(),
                        message->easy_handle),
                    CURLM_OK);

                // Memory cleanup.
                for (auto& poll : continuation.polls_) {
                  if (uv_is_active((uv_handle_t*) poll)) {
                    uv_poll_stop(poll);
                  }
                  uv_close(
                      (uv_handle_t*) poll,
                      [](uv_handle_t* handle) {
                        delete handle;
                      });
                }

                continuation.polls_.clear();

                // We don't have to check uv_is_active for timer since
                // libuv checks it by itself.
                // Return value is always 0.
                uv_timer_stop(&continuation.timer_);
                uv_close(
                    (uv_handle_t*) &continuation.timer_,
                    [](uv_handle_t* handle) {
                      auto& continuation = *(Continuation*) handle->data;
                      continuation.closed_ = true;

                      if (!continuation.error_) {
                        // Build headers map.
                        std::stringstream headers_buffer_stringstream(
                            continuation.headers_buffer_.Extract());
                        std::map<std::string, std::string> headers;

                        // Typical 'headers_buffer_stringstream'
                        // looks like this:
                        // --------------------------------
                        // HTTP/1.1 200
                        // SomeHeaderKey1: SomeHeaderValue1
                        // SomeHeaderKey2: SomeHeaderValue2
                        // --------------------------------
                        while (!headers_buffer_stringstream.eof()) {
                          std::string line;
                          std::getline(headers_buffer_stringstream, line);

                          // Find where ':' is.
                          auto column_iterator = std::find(
                              line.cbegin(),
                              line.cend(),
                              ':');

                          // Skip lines like 'HTTP/1.1 200' that aren't headers.
                          if (column_iterator == line.cend()) {
                            continue;
                          }

                          // Assign key and value.
                          auto key = std::string(
                              line.cbegin(),
                              column_iterator);
                          auto value = std::string(
                              column_iterator + 1,
                              line.cend());

                          // Helper lambda for removing leading
                          // and trailing spaces.
                          // TODO: use absl.
                          static auto trim = [](std::string&& string) {
                            auto start_iterator = string.begin();
                            while (start_iterator != string.end()
                                   && std::isspace(*start_iterator)) {
                              start_iterator++;
                            }

                            auto end_iterator = string.end();
                            do {
                              end_iterator--;
                            } while (std::distance(
                                         start_iterator,
                                         end_iterator)
                                         > 0
                                     && std::isspace(*end_iterator));

                            return std::string(
                                start_iterator,
                                end_iterator + 1);
                          };

                          // Remove leading and trailing spaces.
                          key = trim(std::move(key));
                          value = trim(std::move(value));

                          // Add key and value to the map.
                          // RFC 7230, section 3.2.2:
                          // A recipient MAY combine multiple header fields
                          // with the same field name into one
                          // "field-name: field-value" pair, without changing
                          // the semantics of the message, by appending each
                          // subsequent field value to the combined field
                          // value in order, separated by a comma. The order
                          // in which header fields with the same field name
                          // are received is therefore significant to the
                          // interpretation of the combined field value;
                          // a proxy MUST NOT change the order of these field
                          // values when forwarding a message.
                          //
                          // NOTE: If user tries to add an already
                          // existing header, append the new one
                          // to the old one using comma.
                          // Example:
                          // Cookie: cookie1=value1, cookie2=value2
                          auto iterator = headers.find(key);
                          if (iterator == headers.end()) {
                            // Header doesn't exist yet.
                            headers.emplace(std::move(key), std::move(value));
                          } else {
                            // Header already exists.
                            headers[key] += ", ";
                            headers[key] += std::move(value);
                          }
                        }

                        continuation.k_.Start(Response{
                            continuation.code_,
                            std::move(headers),
                            continuation.body_buffer_.Extract()});
                      } else {
                        continuation.k_.Fail(
                            std::runtime_error(
                                curl_easy_strerror(
                                    (CURLcode) continuation.error_)));
                      }
                    });
              };

              static auto poll_callback = [](uv_poll_t* handle,
                                             int status,
                                             int events) {
                auto& continuation = *(Continuation*) handle->data;

                int flags = 0;
                if (status < 0) {
                  flags = CURL_CSELECT_ERR;
                }
                if (status == 0 && (events & UV_READABLE)) {
                  flags |= CURL_CSELECT_IN;
                }
                if (status == 0 && (events & UV_WRITABLE)) {
                  flags |= CURL_CSELECT_OUT;
                }

                // Getting underlying socket desriptor from poll handle.
                uv_os_fd_t socket_descriptor;
                uv_fileno(
                    (uv_handle_t*) handle,
                    &socket_descriptor);

                // Stores the amount of running easy handles.
                // Set by curl_multi_socket_action.
                int running_handles = 0;

                // Perform an action for the particular socket
                // which is the one we are currently working with.
                // We don't want to perform an action on every socket inside
                // libcurl - only that one.
                curl_multi_socket_action(
                    continuation.multi_.get(),
                    (curl_socket_t) socket_descriptor,
                    flags,
                    &running_handles);

                // If 0 - finalize the transfer.
                if (running_handles == 0) {
                  check_multi_info(continuation);
                }
              };

              static auto timer_callback = [](uv_timer_t* handle) {
                auto& continuation = *(Continuation*) handle->data;

                // Stores the amount of running easy handles.
                // Set by curl_multi_socket_action.
                int running_handles = 0;

                // Called with CURL_SOCKET_TIMEOUT to
                // perform an action with each and every socket
                // currently in use by libcurl.
                curl_multi_socket_action(
                    continuation.multi_.get(),
                    CURL_SOCKET_TIMEOUT,
                    0,
                    &running_handles);

                // If 0 - finalize the transfer.
                if (running_handles == 0) {
                  check_multi_info(continuation);
                }
              };

              static auto socket_function = +[](CURL* easy,
                                                curl_socket_t sockfd,
                                                int what,
                                                Continuation* continuation,
                                                void* socket_poller) {
                int events = 0;

                switch (what) {
                  case CURL_POLL_IN:
                  case CURL_POLL_OUT:
                  case CURL_POLL_INOUT:
                    // Add poll handle for this particular socket.
                    if (what & CURL_POLL_IN) {
                      events |= UV_READABLE;
                    }
                    if (what & CURL_POLL_OUT) {
                      events |= UV_WRITABLE;
                    }

                    // If no poll handle is assigned to this socket.
                    if (socket_poller == nullptr) {
                      socket_poller = new uv_poll_t();
                      continuation->polls_.push_back(
                          (uv_poll_t*) socket_poller);

                      CHECK_EQ(
                          uv_poll_init_socket(
                              continuation->loop_,
                              (uv_poll_t*) socket_poller,
                              sockfd),
                          0);

                      uv_handle_set_data(
                          (uv_handle_t*) socket_poller,
                          continuation);

                      // Assign created poll handle so that in the future
                      // we can get it through socket_poller argument.
                      // Useful to check if we already have a poll handle
                      // for the socket currently in use.
                      CHECK_EQ(
                          curl_multi_assign(
                              continuation->multi_.get(),
                              sockfd,
                              socket_poller),
                          CURLM_OK);
                    }

                    // Stops poll handle if it was started.
                    if (uv_is_active((uv_handle_t*) socket_poller)) {
                      CHECK_EQ(
                          uv_poll_stop(
                              (uv_poll_t*) socket_poller),
                          0);
                    }

                    CHECK_EQ(
                        uv_poll_start(
                            (uv_poll_t*) socket_poller,
                            events,
                            poll_callback),
                        0);

                    break;
                  case CURL_POLL_REMOVE:
                    // Remove poll handle for this particular socket.
                    uv_poll_stop((uv_poll_t*) socket_poller);
                    uv_close(
                        (uv_handle_t*) socket_poller,
                        [](uv_handle_t* handle) {
                          delete (uv_poll_t*) handle;
                        });

                    // Remove this poll handle from vector.
                    for (auto it = continuation->polls_.begin();
                         it != continuation->polls_.end();
                         it++) {
                      if (*it == (uv_poll_t*) socket_poller) {
                        continuation->polls_.erase(it);
                        break;
                      }
                    }

                    // Remove assignment of poll handle to this socket.
                    CHECK_EQ(
                        curl_multi_assign(
                            continuation->multi_.get(),
                            sockfd,
                            nullptr),
                        CURLM_OK);
                    break;
                }
              };

              // Used by libcurl to set a timer after
              // which we should start checking handles inside libcurl.
              static auto timer_function = +[](CURLM* multi,
                                               long timeout_ms,
                                               Continuation* continuation) {
                if (timeout_ms < 0) {
                  timeout_ms = 0;
                }

                uv_timer_start(
                    &continuation->timer_,
                    timer_callback,
                    timeout_ms,
                    0);
              };

              // https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
              static auto write_function = +[](char* data,
                                               size_t size,
                                               size_t nmemb,
                                               Continuation* continuation) {
                continuation->body_buffer_ += std::string(data, size * nmemb);

                return nmemb * size;
              };

              // https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
              static auto header_function = +[](char* data,
                                                size_t size,
                                                size_t nmemb,
                                                Continuation* continuation) {
                continuation->headers_buffer_ += std::string(
                    data,
                    size * nmemb);

                return nmemb * size;
              };

              using std::chrono::duration_cast;
              using std::chrono::milliseconds;

              // CURL multi options.
              CHECK_EQ(
                  curl_multi_setopt(
                      multi_.get(),
                      CURLMOPT_SOCKETDATA,
                      this),
                  CURLM_OK);
              CHECK_EQ(
                  curl_multi_setopt(
                      multi_.get(),
                      CURLMOPT_SOCKETFUNCTION,
                      socket_function),
                  CURLM_OK);
              CHECK_EQ(
                  curl_multi_setopt(
                      multi_.get(),
                      CURLMOPT_TIMERDATA,
                      this),
                  CURLM_OK);
              CHECK_EQ(
                  curl_multi_setopt(
                      multi_.get(),
                      CURLMOPT_TIMERFUNCTION,
                      timer_function),
                  CURLM_OK);

              // CURL easy options.
              if (request_.verify_peer()) {
                CHECK_EQ(
                    curl_easy_setopt(
                        easy_.get(),
                        CURLOPT_SSL_VERIFYPEER,
                        request_.verify_peer().value()),
                    CURLE_OK);
              }

              switch (request_.method()) {
                case Method::GET:
                  CHECK_EQ(
                      curl_easy_setopt(
                          easy_.get(),
                          CURLOPT_HTTPGET,
                          1),
                      CURLE_OK);
                  break;
                case Method::POST:
                  // Converting PostFields.
                  std::unique_ptr<
                      CURLU,
                      decltype(&curl_url_cleanup)>
                      curl_url_handle(
                          curl_url(),
                          &curl_url_cleanup);
                  CHECK_EQ(
                      curl_url_set(
                          curl_url_handle.get(),
                          CURLUPART_URL,
                          request_.uri().c_str(),
                          0),
                      CURLUE_OK);
                  for (const auto& field : request_.fields()) {
                    std::string combined =
                        field.first
                        + '='
                        + field.second;
                    CHECK_EQ(
                        curl_url_set(
                            curl_url_handle.get(),
                            CURLUPART_QUERY,
                            combined.c_str(),
                            CURLU_APPENDQUERY | CURLU_URLENCODE),
                        CURLUE_OK);
                  }
                  char* url_string = nullptr;
                  CHECK_EQ(
                      curl_url_get(
                          curl_url_handle.get(),
                          CURLUPART_QUERY,
                          &url_string,
                          0),
                      CURLUE_OK);
                  fields_string_ = std::unique_ptr<
                      char,
                      decltype(&curl_free)>(
                      url_string,
                      &curl_free);
                  // End of conversion.

                  CHECK_EQ(
                      curl_easy_setopt(
                          easy_.get(),
                          CURLOPT_HTTPPOST,
                          1),
                      CURLE_OK);
                  CHECK_EQ(
                      curl_easy_setopt(
                          easy_.get(),
                          CURLOPT_POSTFIELDS,
                          fields_string_.get()),
                      CURLE_OK);

                  break;
              }

              // Transform 'Request' headers to curl's linked list.
              for (const auto& [key, value] : request_.headers()) {
                std::string header = key + ": " + value;

                // We should only be adding the headers once, so they
                // shouldn't yet exist!
                CHECK(!curl_headers_)
                    << "not expecting to have already allocated headers";

                curl_slist* list = nullptr;

                // 'curl_slist_append()' copies 'header' so we don't
                //  have to worry about its lifetime.
                list = curl_slist_append(list, header.c_str());

                curl_headers_ = std::unique_ptr<
                    curl_slist,
                    decltype(&curl_slist_free_all)>(
                    CHECK_NOTNULL(list),
                    &curl_slist_free_all);
              }

              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_HTTPHEADER,
                      curl_headers_.get()),
                  CURLE_OK);
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_URL,
                      request_.uri().c_str()),
                  CURLE_OK);
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_WRITEDATA,
                      this),
                  CURLE_OK);
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_WRITEFUNCTION,
                      write_function),
                  CURLE_OK);
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_HEADERDATA,
                      this),
                  CURLE_OK);
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_HEADERFUNCTION,
                      header_function),
                  CURLE_OK);
              // Option to follow redirects.
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_FOLLOWLOCATION,
                      1),
                  CURLE_OK);
              // The internal mechanism of libcurl to provide timeout support.
              // Not accurate at very low values.
              // 0 means that transfer can run indefinitely.
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_TIMEOUT_MS,
                      duration_cast<milliseconds>(request_.timeout())),
                  CURLE_OK);
              // If onoff is 1, libcurl will not use any functions that install
              // signal handlers or any functions that cause signals to be sent
              // to the process. This option is here to allow multi-threaded
              // unix applications to still set/use all timeout options etc,
              // without risking getting signals.
              // More here: https://curl.se/libcurl/c/CURLOPT_NOSIGNAL.html
              CHECK_EQ(
                  curl_easy_setopt(
                      easy_.get(),
                      CURLOPT_NOSIGNAL,
                      1),
                  CURLE_OK);

              // Start handling connection.
              CHECK_EQ(
                  curl_multi_add_handle(
                      multi_.get(),
                      easy_.get()),
                  CURLM_OK);
            }
          },
          &context_);
    }

    template <typename Error>
    void Fail(Error&& error) {
      // TODO(benh): avoid allocating on heap by storing args in
      // pre-allocated buffer based on composing with Errors.
      using Tuple = std::tuple<decltype(this), Error>;
      auto tuple = std::make_unique<Tuple>(
          this,
          std::forward<Error>(error));

      // Submitting to event loop to avoid race with interrupt.
      loop_.Submit(
          [tuple = std::move(tuple)]() {
            std::apply(
                [](auto* continuation, auto&&... args) {
                  auto& k_ = continuation->k_;
                  k_.Fail(std::forward<decltype(args)>(args)...);
                },
                std::move(*tuple));
          },
          &context_);
    }

    void Stop() {
      // Submitting to event loop to avoid race with interrupt.
      loop_.Submit(
          [this]() {
            k_.Stop();
          },
          &context_);
    }

    void Register(Interrupt& interrupt) {
      k_.Register(interrupt);

      handler_.emplace(&interrupt, [this]() {
        loop_.Submit(
            [this]() {
              if (!started_) {
                CHECK(!completed_ && !error_);
                completed_ = true;
                k_.Stop();
              } else if (!completed_) {
                CHECK(started_ && !error_);
                completed_ = true;

                for (auto& poll : polls_) {
                  if (uv_is_active((uv_handle_t*) poll)) {
                    uv_poll_stop(poll);
                  }
                  uv_close(
                      (uv_handle_t*) poll,
                      [](uv_handle_t* handle) {
                        delete handle;
                      });
                }
                polls_.clear();

                // We don't have to check uv_is_active for timer since
                // libuv checks it by itself.
                // Return value is always 0.
                uv_timer_stop(&timer_);
                uv_close(
                    (uv_handle_t*) &timer_,
                    [](uv_handle_t* handle) {
                      auto& continuation = *(Continuation*) handle->data;
                      continuation.closed_ = true;

                      continuation.k_.Stop();
                    });

                CHECK_EQ(
                    curl_multi_remove_handle(
                        multi_.get(),
                        easy_.get()),
                    CURLM_OK);
              }
            },
            &interrupt_context_);
      });

      // NOTE: we always install the handler in case 'Start()'
      // never gets called.
      handler_->Install();
    }

   private:
    EventLoop& loop_;

    Request request_;

    // Stores converted PostFields as a C string.
    std::unique_ptr<char, decltype(&curl_free)> fields_string_;

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy_;
    std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> multi_;
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> curl_headers_;

    uv_timer_t timer_;
    std::vector<uv_poll_t*> polls_;

    // Response variables.
    long code_ = 0;
    EventLoop::Buffer headers_buffer_;
    EventLoop::Buffer body_buffer_;

    bool started_ = false;
    bool completed_ = false;
    bool closed_ = false;

    int error_ = 0;

    // NOTE: we use 'context_' in each of 'Start()', 'Fail()', and
    // 'Stop()' because only one of them will called at runtime.
    Scheduler::Context context_;
    Scheduler::Context interrupt_context_;

    std::optional<Interrupt::Handler> handler_;

    // NOTE: we store 'k_' as the _last_ member so it will be
    // destructed _first_ and thus we won't have any use-after-delete
    // issues during destruction of 'k_' if it holds any references or
    // pointers to any (or within any) of the above members.
    K_ k_;
  };

  struct Composable final {
    template <typename Arg>
    using ValueFrom = Response;

    template <typename Arg, typename Errors>
    using ErrorsFrom = tuple_types_union_t<
        Errors,
        std::tuple<std::runtime_error>>;

    template <typename Arg, typename K>
    auto k(K k) && {
      return Continuation<K>(std::move(k), loop_, std::move(request_));
    }

    EventLoop& loop_;
    Request request_;
  };
};

////////////////////////////////////////////////////////////////////////

inline auto Client::Do(Request&& request) {
  // TODO(benh): need 'Client::Default()'.
  auto& loop = EventLoop::Default();

  if (verify_peer_.has_value() && !request.verify_peer().has_value()) {
    request.verify_peer_ = verify_peer_;
  }

  if (certificate_.has_value() && !request.certificate().has_value()) {
    request.certificate_ = certificate_;
  }

  // NOTE: we use a 'RescheduleAfter()' to ensure we use current
  // scheduling context to invoke the continuation after the transfer has
  // completed (or was interrupted).
  return RescheduleAfter(
      // TODO(benh): borrow '&loop' so http call can't outlive a loop.
      _HTTP::Composable{loop, std::move(request)});
}

////////////////////////////////////////////////////////////////////////

inline auto Client::Get(
    std::string&& uri,
    std::chrono::nanoseconds&& timeout) {
  return Do(
      Request::Builder()
          .uri(std::move(uri))
          .method(GET)
          .timeout(std::move(timeout))
          .Build());
}

////////////////////////////////////////////////////////////////////////

inline auto Client::Post(
    std::string&& uri,
    PostFields&& fields,
    std::chrono::nanoseconds&& timeout) {
  return Do(
      Request::Builder()
          .uri(std::move(uri))
          .method(POST)
          .timeout(std::move(timeout))
          .fields(std::move(fields))
          .Build());
}

////////////////////////////////////////////////////////////////////////

inline auto Get(
    std::string&& url,
    std::chrono::nanoseconds&& timeout = std::chrono::nanoseconds(0)) {
  return Client().Get(std::move(url), std::move(timeout));
}

////////////////////////////////////////////////////////////////////////

inline auto Post(
    std::string&& url,
    PostFields&& fields,
    std::chrono::nanoseconds&& timeout = std::chrono::nanoseconds(0)) {
  return Client().Post(std::move(url), std::move(fields), std::move(timeout));
}

////////////////////////////////////////////////////////////////////////

} // namespace http
} // namespace eventuals

////////////////////////////////////////////////////////////////////////
