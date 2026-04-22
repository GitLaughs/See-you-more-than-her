#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
using namespace std::chrono_literals;

//通过相机内参标定获取相机内参矩阵以及畸变系数
std::array<double,9> camera_matrix={517.91673, 0.0, 307.79504, 
                                    0.0, 517.26799, 252.8288, 
                                    0.0, 0.0, 1.0};
std::array<double,12> camera_matrix_p={514.04413, 0.0, 298.4275, 0.0,
                                    0.0, 520.72217, 253.49136,0.0, 
                                    0.0, 0.0, 1.0,0.0};
std::vector<double> dis{0.030513,-0.093904, 0.002366,-0.013578, 0.00000}; 








class camera_info: public rclcpp::Node
{
    public:
        camera_info():Node("camera_info_pub")
        {
            info_pub=create_publisher<sensor_msgs::msg::CameraInfo>("/camera/color/astra/camera_info",1);
            time=create_wall_timer(50ms,std::bind(&camera_info::time_callback,this));
        }
    private:
        rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub;
        rclcpp::TimerBase::SharedPtr time;
        void time_callback()//以50ms的频率发布相机内参话题
        {
            auto message=sensor_msgs::msg::CameraInfo();
            message.k=camera_matrix;
            message.d=dis;
            message.p=camera_matrix_p;
            info_pub->publish(message);
        }


};
int main(int argc, char const *argv[])
{
    rclcpp::init(argc,argv);
    auto camera_info_node=std::make_shared<camera_info>();
    rclcpp::spin(camera_info_node);
    rclcpp::shutdown();
    return 0;
}
