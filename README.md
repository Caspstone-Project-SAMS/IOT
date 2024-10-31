# Student Attendance Management System (SAMS) - IOT Project
This is a part of Capstone Project, the goal of this project is to design and develop a Fingerprint module/IOT device which plays an important role in resolve the main problem and fullfill the flows of the system.

## Contributer
* Le Dang Khoa  - <a href="https://github.com/khoaLe12">GitHub</a>

## Tech Stack
* Language: Arduino C/C++
* Tool: Arduino IDE
* Others: Wifi module, HTTPS, Websocket, Fingerprint scanner

## Description
The system is designed and developed to establish and maintain communication connections between IoT devices and servers. To achieve that, on the IoT side, we use an ESP8266 NodeMCU microcontroller with a built-in Wi-Fi module. This helps the device connect to the server, transmit data over the Internet, and create real-time communication using the HTTPS and Websocket protocols. 

Each IoT device participating in the system will perform a specific mode. There are two available modes called "fingerprint collection" and "attendance". Both modes must 
be implemented using the WebSocket protocol to create a bi-directional communication with the server. To keep the connection alive during operation, we developed a mechanism that periodically checks the connection status and reconnects if it is closed.

1. With the mode "fingerprint collection", the device can be used to register and update student fingerprints. The programming flowchart of this mode is shown in [Fig. 0](#fingerprint-collection-mode-flowchart). According to the flowchart, at the start, the device will perform some processes to set up device components, Wi-Fi, server connection, and configuration. In each iteration, it displays the date-time data retrieved from RTC module and perform periodic time updates. During the loop, module checks the connection status and listen for websocket event. If the module receives event "RegisterFingerprint" or "UpdateFingerprint" with the information of enrolling student, it will start collecting fingerprint templates, uploading fingerprint data and notifying to the server when the process is finished.

2. In "attendance" mode, the module is used to take attendance by scanning fingerprints. The programming flowchart for this mode, shown in [Fig. 1](#attendance-mode-flowchart), details the logic handling process. To download fingerprint templates and schedule data from the server, we created a process that checks if it is a "preparation" event. During this event, the module downloads schedule details, including class, student information and their fingerprint data. To record and store attendance results, we developed a data structure that captures the relationships between objects: each schedule contains a list of attendance reports associated with students, and each student has a list of stored fingerprint IDs. The preparation process is monitored by the server, tracking its state and percentage of completion. Once the preparation is complete, the module can scan fingerprints to take attendance through the "authentication process." By utilizing date-time information from the RTC module, we determine if a schedule is ongoing by comparing its time frame with the current date and time. This ensures that the "authentication process" is only available during the designated time frame. When a student is successfully authenticated, the module records the attendance status and uploads it to the server.

## Diagram
### Flowchart
#### Fingerprint Collection Mode
<a name="fingerprint-collection-mode-flowchart"></a>
![Fig. 0: Fingerprint Collection Mode Flowchart](https://github.com/khoaLe12/Public-Image/blob/main/fingerprint_collection_mode_flowchart.png)

#### Attendance Mode
<a name="attendance-mode-flowchart"></a>
![Fig. 1: Attendance Mode Flowchart](https://github.com/khoaLe12/Public-Image/blob/main/attendance_mode_flowchart.png)


### Block Diagram
![File Structure](https://github.com/khoaLe12/Public-Image/blob/main/iot_blockdiagram.png)


### Circuit schematic/hardware interfacing
![File Structure](https://github.com/khoaLe12/Public-Image/blob/main/iot_schematic.jpg)


### Hardware System
![File Structure](https://github.com/khoaLe12/Public-Image/blob/main/hardware_system.jpg)
