#include "server_networking/network.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include "utility/fps_camera/fps_camera.hpp"
#include "utility/periodic_signal/periodic_signal.hpp"

struct MouseUpdate
{
    float mouse_x_pos;
    float mouse_y_pos;
};

struct GameUpdate
{
    float yaw;
    float pitch;
};

MouseUpdate mouse_update;

int main(){

    FPSCamera fps_camera;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("network_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    Network server(7777, sinks);

    unsigned int test_client_id = -1;

    std::function<void(unsigned int)> on_client_connect = [&](unsigned int client_id) {
        /*physics.create_character(client_id);*/
        /*connected_character_data[client_id] = {client_id};*/
        spdlog::info("just registered a client with id {}", client_id);
        // TODO no need to broadcast this to everyone, only used once on the connecting client
        server.reliable_send(client_id, &client_id, sizeof(unsigned int));
        test_client_id = client_id;
    };
    
    server.set_on_connect_callback(on_client_connect);
    server.initialize_network();

    std::vector<MouseUpdate> mouse_updates_this_tick;

    PeriodicSignal periodic_signal(30);

    while (true) {
        mouse_updates_this_tick.clear();
        std::vector<PacketWithSize> packets = server.get_network_events_since_last_tick();
        for (PacketWithSize pws : packets){
            MouseUpdate mu;
            std::memcpy(&mu, pws.data.data(), sizeof(MouseUpdate));
            mouse_updates_this_tick.push_back(mu);
            fps_camera.mouse_callback(mu.mouse_x_pos, mu.mouse_y_pos);
            std::cout << fps_camera.transform.get_string_repr() << std::endl;
        }
        GameUpdate game_update;
        game_update.pitch = fps_camera.transform.rotation.x;
        game_update.yaw = fps_camera.transform.rotation.y;
        if (periodic_signal.process_and_get_signal()){
            if (test_client_id != -1)
            {
                server.unreliable_send(test_client_id, &game_update, sizeof(GameUpdate));
            }
            
        }        
    }


    return 0;
}
