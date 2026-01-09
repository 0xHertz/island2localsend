#!/bin/bash

echo "安装 Island 2 LocalSend..."

# 编译程序
echo "编译程序..."
g++ -std=c++17 island2localsend.cpp -o island2localsend \
    `pkg-config --cflags --libs gtkmm-3.0` \
    `pkg-config --cflags --libs ayatana-appindicator3-0.1` \
    `pkg-config --cflags --libs gio-2.0`

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

# 安装可执行文件
echo "安装可执行文件到 /usr/local/bin/"
sudo cp island2localsend /usr/local/bin/
sudo chmod +x /usr/local/bin/island2localsend

# 安装图标
echo "安装图标..."
sudo mkdir -p ~/.local/share/icons/hicolor/scalable/apps/
sudo cp icons/island-sender.svg ~/.local/share/icons/hicolor/scalable/apps/
sudo cp -r icons/hicolor/* ~/.local/share/icons/hicolor/

# 更新图标缓存
echo "更新图标缓存..."
sudo gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor/

# 安装桌面文件
echo "安装桌面文件..."
sudo cp island2localsend.desktop ~/.local/share/applications/
sudo chmod 644 ~/.local/share/applications/island2localsend.desktop

# 安装自动启动文件（可选）
#echo "配置自动启动..."
#mkdir -p ~/.config/autostart/
#cp island-sender.desktop ~/.config/autostart/

# 安装应用数据文件
#echo "安装应用数据文件..."
#sudo mkdir -p /usr/share/island-sender/
#sudo cp -r icons/ /usr/share/island-sender/

echo "安装完成！"
echo "你可以在应用菜单中找到 'LocalSend Island Sender'"
echo "程序会自动添加到开机启动项"
