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
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/fps_camera/fps_camera.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/texture_packer/texture_packer.hpp"

#include <filesystem>

#include "graphics/cube_map/cube_map.hpp"
#include "graphics/batcher/generated/batcher.hpp"

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

    std::vector<ShaderType> rq_shaders = {ShaderType::CWL_V_TRANSFORMATION_TEXTURE_PACKED};   

    ShaderCache shader_cache(rq_shaders);

    Batcher batcher(shader_cache);

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

    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_TEXTURE_PACKED, ShaderUniformVariable::LOCAL_TO_WORLD, glm::mat4(1));
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_TEXTURE_PACKED, ShaderUniformVariable::WORLD_TO_CAMERA, glm::mat4(1));

    std::vector<GameUpdate> game_updates_this_tick;

    FPSCamera fps_camera();

    const std::filesystem::path textures_directory = "assets";
    std::filesystem::path output_dir = std::filesystem::path("assets") / "packed_textures";
    int container_side_length = 1024;

    TexturePacker texture_packer(textures_directory, output_dir, container_side_length);

    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_TEXTURE_PACKED,
                             ShaderUniformVariable::PACKED_TEXTURE_BOUNDING_BOXES,
                             texture_packer.texture_index_to_bounding_box);

    std::filesystem::path cube_map_dir = std::filesystem::path("assets") / "skybox";
    std::string file_extension = "png";

    CubeMap skybox(cube_map_dir, file_extension, texture_packer);

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
            fps_camera.transform.rotation.x = game_update.pitch;
            fps_camera.transform.rotation.y = game_update.yaw;
            fps_camera.transform.print();
            std::cout << "yaw = " << game_update.yaw << std::endl << "pitch = " << game_update.pitch << std::endl;
        }
        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_TEXTURE_PACKED, ShaderUniformVariable::CAMERA_TO_CLIP, fps_camera.get_view_matrix());  
        
        // draw skybox
        // glDepthMask(GL_FALSE);
        // glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when values are equal to depth
        //                         // buffer's content
        std::vector<std::tuple<int, const IVPTexturePacked &>> skybox_faces = {
            {0, skybox.top_face},  {1, skybox.bottom_face}, {2, skybox.right_face},
            {3, skybox.left_face}, {4, skybox.front_face},  {5, skybox.back_face}};

        for (const auto &[id, face] : skybox_faces) {
            std::vector<int> ptis(face.xyz_positions.size(), face.packed_texture_index);
            std::vector<int> ptbbi(face.xyz_positions.size(), face.packed_texture_bounding_box_index);

            // Call to the actual function
            batcher.cwl_v_transformation_texture_packed_shader_batcher.queue_draw(
                id, face.indices, face.xyz_positions, ptis, face.packed_texture_coordinates, ptbbi);
        }

        batcher.cwl_v_transformation_texture_packed_shader_batcher.draw_everything();

        // glDepthFunc(GL_LESS);
        // glDepthMask(GL_TRUE);
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