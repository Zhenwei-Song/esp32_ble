该项目为基于esp32的蓝牙自组网实现。

入网过程已经实现并已验证。

注意：
仅gatt_server_service_table_modified下文件为项目文件


TODO:
路由表的维护
链路的维护（断裂后重连）
断电重启后的重连

已知问题：
接收蓝牙包使用的插入链表过程存在问题，原因未知。
![image](https://github.com/Zhenwei-Song/esp32_ble/assets/124581711/17f6acd4-e8c8-4179-86c8-628ebc8eb562)

仅供学习交流使用

Copyright (c) 2024 by Zhenwei Song, All Rights Reserved.
