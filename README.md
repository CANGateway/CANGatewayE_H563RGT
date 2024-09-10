# CANGateway_H563RGT

# 開発環境
- STM32CubeIDE v1.14.0 (v.1.15.0では再起動後にEthernetが動作しないバグが出たため)

# 動作確認環境
- Ubuntu22.04
- can-utils
- cannelloni

can-utilsのインストール
```
sudo apt install can-utils
```

cannelloniのインストール
```
git clone https://github.com/mguentner/cannelloni
cd cannelloni
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```

# 仕様

| 説明 | 値 |
| :-- | :-- |
| CANビットレート | 1Mbps |
| IPアドレス | 192.168.1.20  |
| CAN1用ポート番号 | 6000 |
| CAN2用ポート番号 | 6001 |

# 動作確認
CAN1とCAN2をジャンパ線で繋いで相互に通信できることを確認する方法

## インターフェースの定義
```
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
sudo ip link add dev vcan1 type vcan
sudo ip link set up vcan1
```

## cannelloniでポートを開いて通信開始
ターミナルを2つ開きます

ターミナル1
```
cannelloni -I vcan0 -C c -R 192.168.1.20 -r 6000
```

ターミナル2
```
cannelloni -I vcan1 -C c -R 192.168.1.20 -r 6001
```

## 通信の動作確認
vcan0を通してCAN1にメッセージを書き込み、CAN2で受信し、vcan1に返ってくることを確認します  
先ほどとは別にターミナルを2つ開きます

ターミナル3
```
candump vcan1
```

ターミナル4
```
cansend vcan0 1FF#1122334455667788
```

ターミナル4からcansendコマンドを送るたびにターミナル3に以下のように受信したメッセージが表示されれば成功です

```
$ candump vcan1
  vcan1  1FF   [8]  11 22 33 44 55 66 77 88
  vcan1  1FF   [8]  11 22 33 44 55 66 77 88
  vcan1  1FF   [8]  11 22 33 44 55 66 77 88
  vcan1  1FF   [8]  11 22 33 44 55 66 77 88
```