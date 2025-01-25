#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include "networking/client_networking/network.hpp"
#include "utility/fixed_frequency_loop/fixed_frequency_loop.hpp"
// for window
#include <glad/glad.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "graphics/window/window.hpp"

unsigned int SCREEN_WIDTH = 800;
unsigned int SCREEN_HEIGHT = 600;

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

/*
void get_mouse_position(sf::RenderWindow& window, Network& network){
    sf::Vector2i position = sf::Mouse::getPosition();
   // std::cout << "Mouse Position: (" << mouse_position.x <<", " << mouse_position.y << ")";
   network.send_packet(position, sizeof(position), false); // false for UDP?
}

bool on_window_close(sf::RenderWindow& window){
    return !window.isOpen();
}
*/

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    // std::cout << "(" << xpos << ", " << ypos << ")" << std::endl;
    mouse_update.mouse_x_pos = xpos;
    mouse_update.mouse_y_pos = ypos;
}

int main(){

    GLFWwindow *window = initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "glfw window", false,
                                                                false, false);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("network_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    std::string ip_address = "localhost";
    Network network(ip_address, 7777, sinks);

    FixedFrequencyLoop update_mouse_xy_position;
    
    network.initialize_network();
    network.attempt_to_connect_to_server();

    std::vector<GameUpdate> game_updates_this_tick;

    std::function<void(double)> rate_limited_func = [&](double dt){ 
        network.send_packet(&mouse_update, sizeof(MouseUpdate), false); // false for UDP?
        MouseUpdate mu;
        std::memcpy(&mu, &mouse_update, sizeof(MouseUpdate));
        std::cout << "(" << mu.mouse_x_pos << ", " << mu.mouse_y_pos << ")" << std::endl;
        std::vector<PacketWithSize> packets = network.get_network_events_received_since_last_tick();
        for (PacketWithSize pws : packets){
            GameUpdate game_update;
            std::memcpy(&game_update, pws.data.data(), sizeof(GameUpdate));
            game_updates_this_tick.push_back(game_update);
            // fps_camera.mouse_callback(game_update.mouse_x_pos, mu.mouse_y_pos); for later
            // fps_camera.transform.print();
            std::cout << "yaw = " << game_update.yaw << std::endl << "pitch = " << game_update.pitch << std::endl;
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    };
    std::function<bool()> termination_condition_func = [&](){
        return glfwWindowShouldClose(window);
    };

    // loop needs rate_limited_func and termination condition
    update_mouse_xy_position.start(30, rate_limited_func, termination_condition_func);
        

    return 0;
}