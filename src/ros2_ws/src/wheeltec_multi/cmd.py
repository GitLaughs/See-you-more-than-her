#!/usr/bin/env python
# -*- coding: utf-8 -*-
import paramiko 
import time
import threading


#修改此变量，将你目前要操作的车辆的ip地址输入进去
ip = [
        "192.168.0.100" ,   #第一个ip地址需为主车ip地址，剩余的为从车ip地址
        # "192.168.0.101" , 
        "192.168.0.101" , 
        # "192.168.0.103" ,
        # "192.168.0.104" ,
        # "192.168.0.105" , 
        # "192.168.0.106" , 
        # "192.168.0.107" ,
        # "192.168.0.108" ,
        # "192.168.0.109" ,
        # "192.168.0.110" ,
        # "192.168.0.111" ,
        # "192.168.0.112" ,
        # "192.168.0.113" ,
        # "192.168.0.114" ,
    ]

username = 'wheeltec' 
password = 'dongguan' 
ip_status = []
input_flag=1


def control_onecar(carnub,carip,carcmd):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(hostname=carip, port=22, username=username, password=password)
    except Exception as e:
        print(e)
    time.sleep(2) 
    command = ssh.invoke_shell() 
    if carcmd == 'open' and ip_status[carnub-1] == "stop" and carnub == 1:
        command.send("DISPLAY=:0 bash ~/wheeltec_ros2/src/wheeltec_multi/scripts/leadercar.sh \n")
        time.sleep(6)
        ip_status[carnub-1] = "open"
    elif carcmd == 'open' and ip_status[carnub-1] == "stop" and carnub != 1:
        command.send("DISPLAY=:0 bash ~/wheeltec_ros2/src/wheeltec_multi/scripts/slavecar.sh \n")
        time.sleep(6)
        ip_status[carnub-1] = "open"
    elif carcmd == 'stop':
        command.send("DISPLAY=:0 bash ~/wheeltec_ros2/src/wheeltec_multi/scripts/killall.sh \n")
        time.sleep(6)
        ip_status[carnub-1] = "stop"
    else:
        print("请重新输入")
#    output = command.recv(65535)
#    print(output.decode("utf-8"))
    ssh.close()

#python threading
def control_allcar(carlen,allcarip,carcmd):
    threads = []
    for i in range(carlen):
        t = threading.Thread(target=control_onecar,args=(i+1,allcarip[i],carcmd))
        threads.append(t)
    for i in range(carlen):
        threads[i].start()
    for i in range(carlen):
        threads[i].join()

def print_allcar_status(carip,carstatus):
    print(" ")
    print("当前所有车辆IP地址及运行状态如下：")
    for i in range(len(carip)):
            if i == 0:
                print("%d号主车，编号：%d ip：%s 状态：%s"%(i+1,i+1,carip[i],carstatus[i]))
            else:
                print("%d号从车，编号：%d ip：%s 状态：%s"%(i+1,i+1,carip[i],carstatus[i]))
    print(" ")
    

#主函数
if __name__=="__main__":
    try:
        for i in range(len(ip)):
            ip_status.append("stop")
        print_allcar_status(ip,ip_status)
        makesure = input("1.请确认目前使用的主车ip及从车ip无误，有误请修改cmd.py文件的ip变量，并按回车键确认:")
        print("正在初始化中，请稍后......")
        control_allcar(len(ip),ip,"stop")
        print_allcar_status(ip,ip_status)
        while True:
            print("2.选择你要操作的小车，开启或者结束运行这辆小车的程序")
            thiscarnub = int(input("请输入你要操作的小车编号数字，并按回车键确认（操作所有车开启或关闭程序可输入数字0）:"))
            cmd = str(input("请输入open或者stop开始运行或者结束运行程序,并按回车键确认:"))
            if thiscarnub == 0:
                control_allcar(len(ip),ip,cmd)
            else:
                control_onecar(thiscarnub,ip[thiscarnub-1],cmd)
            print_allcar_status(ip,ip_status)
        
    #运行出现问题则程序终止并打印相关错误信息
    except Exception as e:
        print(e)

    #程序结束前终止所有程序运行
    finally:
        control_allcar(len(ip),ip,"stop")