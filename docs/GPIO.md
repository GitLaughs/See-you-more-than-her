GPIO（ General Purpose Input/Output）是指通用输入输出。它是微处理器或微控制器上的一组引脚，这些引脚没有固定的专用功能。而是可以通过软件编程来决定其用途。用作输入/输出（Input/Output）是它的核心能力。它既可以作为输入，读取外部设备的信号（比如检测按键有没有被按下）；也可以作为输出，输出的电压为1.8V，控制外部设备的状态（比如点亮一颗 LED 灯）。

注意点：A1一共有十一个GPIO引脚（GPIO_PIN0到GPIO_PIN10），其中GPIO_PIN1，3，4，5，6，7已经在使用，不能修改。

对于赛题1，2：可用GPIO为0，2，8，9，10

（其中，如引脚图所示，GPIO_PIN_0默认状态是UART TX0，GPIO_PIN_1默认状态是UART RX0）

引用头文件（GPIO）

开发主要用到的头文件是gpio_api.h，文件位置如下

smartsens_sdk/output/opt/m1_sdk/usr/include/smartsoc/gpio_api.h

其中包含gpio相关api的详细注释

需要调用头文件：

##include "gpio_api.h"         //必须引用，包含所有公共API接口的函数声明

GPIO功能依赖库列表

库文件路径链接方式的cmake等写法可以参考人脸识别ssne_ai_demo中的Paths.cmake和Makefile.txt，以下为使用gpio功能的示例：

依赖库列表：libgpio.so

face_detection\ssne_ai_demo\cmake_config\Paths.cmake中：

set(M1_GPIO_LIB   "${M1_SDK_LIB_DIR}/libgpio.so"   CACHE STRINGINTERNAL)
face_detection\ssne_ai_demo\Makefile.txt中的target_link_libraries函数增加 ${M1_ZLOG_LIB}

target_link_libraries(${PROJECT_NAME} 
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_GPIO_LIB}
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                        ${M1_EMB_LIB}
)
face_detection\ssne_ai_demo\script\run.sh中运行程序前增加载gpio驱动模块：

insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko
API使用示例和注意点

1. gpio_init() - 初始化GPIO设备
示例：

    gpio_handle_t gpio;
    gpio = gpio_init();
    if (gpio == NULL) {
        // 初始化失败
        return -1;
    }
    
    // 使用GPIO...
注意点：

● 必须在使用前调用：所有GPIO操作都需要先调用此函数获取句柄

● 内核模块必须已加载：如果内核模块未加载或设备文件不存在，会返回NULL

2. gpio_close() - 关闭GPIO设备
示例：

int main(void)
{
    gpio_handle_t gpio = gpio_init();
    
    // 使用GPIO...
  
    gpio_close(gpio);  // 释放资源
    return 0;
}
注意点：

● 使用完GPIO后必须调用此函数释放资源，避免资源泄漏

3. gpio_set_enable() - 使能/禁用GPIO模式
示例：

    // 使能GPIO2为GPIO模式
    gpio_set_enable(gpio, GPIO_PIN_2, true);  
    // 禁用GPIO2
    gpio_set_enable(gpio, GPIO_PIN_2, false);
注意点：

● 支持位掩码：可以同时操作多个引脚，如GPIO_PIN_0 | GPIO_PIN_2

● 所有GPIO已经初始默认全部使能，可以不调用

● 引脚范围：只支持GPIO_PIN_0 ，2，8，10，9（赛题1，2）

4. gpio_set_mode() - 设置输入/输出模式
示例：

    // 设置GPIO2为输出模式
    gpio_set_mode(gpio, GPIO_PIN_2, GPIO_MODE_OUTPUT);
    
    // 设置GPIO2为输入模式
    gpio_set_mode(gpio, GPIO_PIN_2, GPIO_MODE_INPUT);
注意点：

● 支持位掩码：可以同时设置多个引脚的模式，如GPIO_PIN_0 | GPIO_PIN_2（不会修改没有设置的PIN，会保持原来的PIN）

● 引脚范围：只支持GPIO_PIN_0 ，2，8，10，9（赛题1，2）

● 使用场景：需要手动控制GPIO方向，或先设置模式再操作

5. gpio_write_pin() - 设置引脚输出电平
示例：

    // 设置GPIO2为高电平
    gpio_write_pin(gpio, GPIO_PIN_2, GPIO_PIN_SET);
    
    // 设置GPIO2为低电平
    gpio_write_pin(gpio, GPIO_PIN_2, GPIO_PIN_RESET);
注意点：

● 此函数使用需要确保已经完成以下操作：

1. 使能GPIO模式（gpio_set_enable()）

2. 设置为输出模式（gpio_set_mode()）

● 支持位掩码：可以同时设置多个引脚，如GPIO_PIN_0 | GPIO_PIN_2同时设置为高电平（1.8V）

● 引脚范围：只支持GPIO_PIN_0 ，2，8，10，9（赛题1，2）

● 参数：GPIO_PIN_SET（高电平）或GPIO_PIN_RESET（低电平）

6. gpio_read_pin() - 读取引脚输入电平
示例：

    uint16_t state;
    // 读取GPIO0的状态
    gpio_read_pin(gpio, GPIO_PIN_0, &state);
    // 检查引脚状态
    if (state & GPIO_PIN_0) {
        printf("GPIO0 is HIGH\n");
    } else {
        printf("GPIO0 is LOW\n");
    }
注意点：

● 此函数需要确保已经完成以下操作：

○ 使能GPIO模式（gpio_set_enable()）

○ 设置为输入模式（gpio_set_mode()）

● 返回值是位掩码：返回的state是位掩码，不是简单的0/1

● 只读取需要读的GPIO

○ 例如：如果PIN0=SET, PIN1=RESET，返回0x0001

● 支持位掩码：可以同时读取多个引脚，如GPIO_PIN_0 | GPIO_PIN_2

● 引脚范围：只支持GPIO_PIN_0 ，2，8，10，9（赛题1，2）

7. gpio_toggle_pin() - 翻转引脚电平
示例：

    // 翻转GPIO2的电平（如果原来是高，变为低；如果原来是低，变为高）
    gpio_toggle_pin(gpio, GPIO_PIN_2);
注意点：

● 此函数需要完成以下操作：

1. 使能GPIO模式（gpio_set_enable()）

2. 设置为输出模式（gpio_set_mode()）

● 支持位掩码：可以同时翻转多个引脚，如GPIO_PIN_0 | GPIO_PIN_2

● 引脚范围：只支持GPIO_PIN_0 ，2，8，10，9（赛题1，2）

● 使用场景：LED闪烁、信号翻转等需要切换电平的场景

8. gpio_set_alternate() - 配置复用功能
示例：

    // 配置GPIO0的复用功能
    // 输出复用：UART_TX0
    gpio_set_alternate(gpio, GPIO_PIN_0, 
                       GPIO_AF_INPUT_NONE, 
                       GPIO_AF_OUTPUT_UART_TX0);
注意点：

● 只支持单个引脚：此函数不支持位掩码，只能配置一个引脚

● 引脚范围：默认状态下，GPIO_PIN_0为TX0，GPIO_PIN_2为RX0，需要设为GPIO输入/输出，输入GPIO_AF_INPUT_NONE和GPIO_AF_OUTPUT_NONE

● 引脚必须是单个：如果传入位掩码（如GPIO_PIN_0 | GPIO_PIN_2），会返回错误

● 保持原值：如果某个复用功能不需要配置，可以使用：

○ GPIO_AF_INPUT_NONE（0xFF）保持输入复用原值

○ GPIO_AF_OUTPUT_NONE（0xFF）保持输出复用原值

● 配置后引脚不再是GPIO：配置复用功能后，引脚将用于其他功能，不再是普通GPIO