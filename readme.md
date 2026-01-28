This project seeks to display the camera metrics of the Raspberry Pi Camera Module 3 and a USB Webcam using Grafana and Prometheus. 

The Pi Camera Module 3 is processed using a Raspberry Pi 5 8 Gb version (pretty powerful!)

The USB Webcam is processed using the new Arduino Uno Q, a collaboration between Qualcomm and Arduino (also very powerful, especially for the package/price).

Most of the coding was done on my MacBook, connecting to the microcontrollers using SSH. I'd like to give my thanks to oh-my-zsh for making the terminal experience even more pleasant. All the git commands were done using the terminal, GitHub desktop was not used at all.

This readme.md file was written (mostly) on my MacBook in neovim from the terminal.

How to get things running:

1. First start by ssh'ing into your target microncontroller(s).

2. Afterwards, you want to start the stream using the mediamtx library in RaspberryPi. More information can be found here:

https://www.raspberrypi.com/documentation/computers/camera_software.html#stream-video-over-a-network-with-rpicam-apps

2b. Start streaming, in my setup I cd into the mediamtx_parent folder and run ./mediamtx (this is assuming you have attached a pi camera module to the pi already)


3a.

You will need to donwnload the prometheus, and node exporter, and chagne the config file in a manner similar to the yml file attached in this git exporter

3b. Run the pi exporter script from the pi: open a new terminal on the pi or ssh, and now cd into the folder where you're holding the pe2 script 

run the following command to compile the cpp files:

g++ -std=c++17 \
    main.cpp \
    mediamtx_fetcher.cpp \
    prometheus_server.cpp \
    -lcurl \
    -o pi_exporter


then run the new pi_exporter executable

4. now open up grafana (resources: https://grafana.com/docs/grafana/latest/setup-grafana/)

Access it through http://<ip-addr>:3000 

5. you can organize the dashboard to your liking.

6. Now, set up the arduino camera 
6b. ssh into arduino 

Download mjpeg_streamer, and use the following command: 

mjpg_streamer \                          
  -i "input_uvc.so -d /dev/video0 -r 640x480 -f 30" \
  -o "output_http.so -p 8080 -w /usr/share/mjpg-streamer/www"

7. compile the me2 exporter:

g++ -std=c++17 me2.cpp -lcurl -o me2

and then run it:

./me2 

everything should be working now! see the atttached demo vidoe for an example 
