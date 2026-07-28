#include <iostream>
#include <filesystem>
#include <chrono>
#include <string>
#include <optional>
#include <thread>
#include <vector>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/signals2.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>

#include "model.h"
#include "model_serialization.h"

namespace net = boost::asio;
namespace sys = boost::system;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = net::ip::tcp;

using namespace std::literals;

// Состояние игры - МИНИМАЛЬНОЕ
struct GameState {
    std::vector<model::Dog> dogs;
    std::unordered_map<std::string, model::Dog::Id> tokens;
    
    GameState() = default;
    
    bool operator==(const GameState& other) const {
        if (dogs.size() != other.dogs.size()) return false;
        if (tokens.size() != other.tokens.size()) return false;
        return true;
    }
};

// Сериализация GameState - ПРОСТАЯ
class GameStateRepr {
public:
    GameStateRepr() = default;
    
    explicit GameStateRepr(const GameState& state) {
        for (const auto& dog : state.dogs) {
            dogs_.emplace_back(dog);
        }
        // Сериализуем токены как отдельные векторы
        for (const auto& [token, dog_id] : state.tokens) {
            tokens_.push_back(token);
            token_dog_ids_.push_back(*dog_id);
        }
    }
    
    GameState Restore() const {
        GameState state;
        for (const auto& dog_repr : dogs_) {
            state.dogs.push_back(dog_repr.Restore());
        }
        for (size_t i = 0; i < tokens_.size(); ++i) {
            state.tokens[tokens_[i]] = model::Dog::Id{token_dog_ids_[i]};
        }
        return state;
    }
    
    template <class Archive>
    void serialize(Archive& ar, unsigned) {
        ar & dogs_;
        ar & tokens_;
        ar & token_dog_ids_;
    }
    
private:
    std::vector<serialization::DogRepr> dogs_;
    std::vector<std::string> tokens_;
    std::vector<uint32_t> token_dog_ids_;
};

// Функции сохранения и загрузки
void SaveState(const GameState& state, const std::string& path) {
    std::string temp_path = path + ".tmp";
    {
        std::ofstream ofs(temp_path);
        boost::archive::text_oarchive oa(ofs);
        GameStateRepr repr(state);
        oa << repr;
    }
    std::filesystem::rename(temp_path, path);
}

GameState LoadState(const std::string& path) {
    std::ifstream ifs(path);
    boost::archive::text_iarchive ia(ifs);
    GameStateRepr repr;
    ia >> repr;
    return repr.Restore();
}

// Игровое приложение
class Application {
public:
    using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds)>;
    
    boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
        return tick_signal_.connect(handler);
    }
    
    void Tick(std::chrono::milliseconds delta) {
        game_time_ += delta;
        tick_signal_(delta);
    }
    
    void SetState(const GameState& state) {
        state_ = state;
    }
    
    GameState GetState() const {
        return state_;
    }
    
    // Присоединение игрока
    std::string JoinGame(const std::string& name, const std::string& map_id) {
        model::Dog::Id dog_id{static_cast<uint32_t>(state_.dogs.size() + 1)};
        model::Dog dog{dog_id, name, {0, 0}, 3};
        state_.dogs.push_back(dog);
        
        std::string token = "token" + std::to_string(*dog_id);
        state_.tokens[token] = dog_id;
        return token;
    }
    
    // Получение состояния игры
    json::object GetGameState(const std::string& token) {
        if (state_.tokens.find(token) == state_.tokens.end()) {
            return {{"error", "Invalid token"}};
        }
        
        json::object response;
        json::array players;
        for (const auto& dog : state_.dogs) {
            json::object player;
            player["name"] = dog.GetName();
            player["id"] = *dog.GetId();
            player["pos"] = json::array{dog.GetPosition().x, dog.GetPosition().y};
            players.push_back(player);
        }
        response["players"] = players;
        response["lostObjects"] = json::array{};
        return response;
    }
    
private:
    GameState state_;
    TickSignal tick_signal_;
    std::chrono::milliseconds game_time_{0};
};

// Наблюдатель для сохранения
class SerializingListener {
public:
    SerializingListener(const std::string& path, std::chrono::milliseconds period)
        : path_(path), period_(period), last_save_time_(std::chrono::milliseconds::zero()) {}
    
    void OnTick(std::chrono::milliseconds game_time) {
        if (period_ != std::chrono::milliseconds::max() && 
            game_time - last_save_time_ >= period_) {
            SaveState(state_, path_);
            last_save_time_ = game_time;
        }
    }
    
    void SaveOnShutdown() {
        SaveState(state_, path_);
    }
    
    void SetState(const GameState& state) { state_ = state; }
    
private:
    std::string path_;
    std::chrono::milliseconds period_;
    std::chrono::milliseconds last_save_time_;
    GameState state_;
};

// HTTP-сервер
class HttpServer {
public:
    HttpServer(net::io_context& ioc, tcp::endpoint endpoint, Application& app)
        : ioc_(ioc), acceptor_(ioc, endpoint), app_(app) {}

    void Run() { DoAccept(); }

private:
    void DoAccept() {
        auto socket = std::make_shared<tcp::socket>(ioc_);
        acceptor_.async_accept(*socket, [this, socket](sys::error_code ec) {
            if (!ec) {
                std::thread([this, socket]() { HandleRequest(*socket); }).detach();
            }
            DoAccept();
        });
    }

    void HandleRequest(tcp::socket& socket) {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        beast::error_code ec;
        http::read(socket, buffer, req, ec);
        if (ec) return;

        http::response<http::string_body> res;
        res.version(11);
        res.set(http::field::content_type, "application/json");

        try {
            if (req.target() == "/api/v1/game/join" && req.method() == http::verb::post) {
                auto body = json::parse(req.body()).as_object();
                std::string name = body["userName"].as_string().c_str();
                std::string map_id = body["mapId"].as_string().c_str();
                
                std::string token = app_.JoinGame(name, map_id);
                uint32_t player_id = *app_.GetState().tokens[token];
                
                json::object response;
                response["authToken"] = token;
                response["playerId"] = player_id;
                
                res.result(http::status::ok);
                res.body() = json::serialize(response);
                
            } else if (req.target() == "/api/v1/game/state" && req.method() == http::verb::get) {
                std::string token;
                if (req.find("authorization") != req.end()) {
                    std::string auth = req["authorization"];
                    if (auth.substr(0, 7) == "Bearer ") {
                        token = auth.substr(7);
                    }
                }
                auto response = app_.GetGameState(token);
                res.result(http::status::ok);
                res.body() = json::serialize(response);
                
            } else if (req.target() == "/api/v1/game/tick" && req.method() == http::verb::post) {
                auto body = json::parse(req.body()).as_object();
                int ms = body["timeDelta"].as_int64();
                app_.Tick(std::chrono::milliseconds(ms));
                res.result(http::status::ok);
                res.body() = "{}";
                
            } else {
                res.result(http::status::not_found);
                res.body() = R"({"error":"Not found"})";
            }
        } catch (const std::exception& e) {
            res.result(http::status::bad_request);
            res.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
        res.prepare_payload();
        http::write(socket, res, ec);
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    Application& app_;
};

int main(int argc, char* argv[]) {
    std::string state_file_path;
    std::optional<std::chrono::milliseconds> save_period;
    uint16_t port = 8080;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--state-file" && i + 1 < argc) {
            state_file_path = argv[++i];
        } else if (arg == "--save-state-period" && i + 1 < argc) {
            save_period = std::chrono::milliseconds(std::stoll(argv[++i]));
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
    }
    
    Application app;
    bool should_save = !state_file_path.empty();
    
    if (should_save && std::filesystem::exists(state_file_path)) {
        try {
            GameState state = LoadState(state_file_path);
            app.SetState(state);
            std::cout << "State loaded from: " << state_file_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading state: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    std::unique_ptr<SerializingListener> listener;
    if (should_save) {
        auto period = save_period.value_or(std::chrono::milliseconds::max());
        listener = std::make_unique<SerializingListener>(state_file_path, period);
        listener->SetState(app.GetState());
        app.DoOnTick([&listener](std::chrono::milliseconds delta) {
            listener->OnTick(delta);
        });
    }
    
    try {
        net::io_context ioc;
        tcp::endpoint endpoint(tcp::v4(), port);
        HttpServer server(ioc, endpoint, app);
        server.Run();
        
        std::cout << "Server started on port " << port << std::endl;
        
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const sys::error_code& ec, int) {
            if (!ec) {
                std::cout << "Shutting down..." << std::endl;
                if (listener) {
                    listener->SetState(app.GetState());
                    listener->SaveOnShutdown();
                }
                ioc.stop();
            }
        });
        
        unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> workers;
        for (unsigned i = 0; i < num_threads; ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }
        for (auto& t : workers) t.join();
        
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
