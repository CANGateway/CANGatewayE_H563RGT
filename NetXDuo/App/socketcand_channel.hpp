//#pragma once
//
//#include "app_netxduo.h"
//#include "main.h"
//#include "netxduo-cpp/tcp_socket.hpp"
//#include "stmbed/can.hpp"
//#include "stmbed/digital_out.hpp"
//#include "threadx-cpp/mutex.hpp"
//#include "threadx-cpp/thread.hpp"
//#include <cstdlib> // for strtoul, strtol
//#include <functional>
//#include <iomanip>
//#include <memory>
//#include <queue>
//#include <sstream>
//#include <string>
//#include <vector>
//
//// Helper function to parse IPv4 string "a.b.c.d" to ULONG
//// Returns 0 on error
//static ULONG parse_ipv4_string(const char *ip_str) {
//    ULONG ip_addr = 0;
//    ULONG byte_val;
//    int shift = 24;
//    char *end_ptr;
//    const char *current_ptr = ip_str;
//
//    for (int i = 0; i < 4; ++i) {
//        byte_val = strtoul(current_ptr, &end_ptr, 10);
//        if (end_ptr == current_ptr || byte_val > 255) { // Conversion failed or invalid byte
//            return 0;
//        }
//        ip_addr |= (byte_val << shift);
//        shift -= 8;
//        if (i < 3) {
//            if (*end_ptr != '.') { // Expect dot separator
//                return 0;
//            }
//            current_ptr = end_ptr + 1;
//        } else {
//            if (*end_ptr != '\0') { // Should be end of string after 4th byte
//                return 0;
//            }
//        }
//    }
//    return ip_addr;
//}
//
//// Socketcandのテキストプロトコル関連
//namespace socketcand {
//namespace protocol {
//
//// CANフレームをsocketcandの <send ...> コマンド文字列に変換 (printfフォーマット修正)
//static std::string format_send_command(const stmbed::CANMessage &msg, const std::string &interface_name) {
//    std::stringstream ss;
//    ss << "<send " << interface_name << " " << std::hex << std::uppercase;
//
//    if (msg.format == stmbed::CANFormat::CANExtended) {
//        ss << std::setw(8) << std::setfill('0') << msg.id;
//    } else {
//        ss << std::setw(3) << std::setfill('0') << msg.id;
//    }
//
//    ss << " " << std::dec << static_cast<int>(msg.size); // DLC
//
//    for (uint8_t i = 0; i < msg.size; ++i) {
//        ss << " " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(msg.data[i]);
//    }
//    ss << ">"
//       << "\n";
//    return ss.str();
//}
//
//static std::string format_open_command(const std::string &interface_name) { return "<open " + interface_name + ">\n"; }
//
//static std::string format_close_command(const std::string &interface_name) {
//    return "<close " + interface_name + ">\n";
//}
//
//// 受信した <frame ...> コマンド文字列をCANMessageに変換 (エラー処理修正)
//static bool parse_frame_command(const std::string &line, std::string &interface_name, stmbed::CANMessage &msg) {
//    std::stringstream ss(line);
//    std::string segment;
//    std::vector<std::string> seglist;
//
//    while (std::getline(ss, segment, ' ')) {
//        seglist.push_back(segment);
//    }
//
//    if (seglist.size() < 4 || seglist[0] != "<frame" || seglist.back().back() != '>') {
//        return false;
//    }
//    seglist.back().pop_back();
//
//    interface_name = seglist[1];
//
//    // IDのパース (例外を投げないstrtoulを使用)
//    char *end_ptr_id;
//    errno = 0; // errnoをクリア
//    unsigned long parsed_id = strtoul(seglist[2].c_str(), &end_ptr_id, 16);
//    if (*end_ptr_id != '\0' || errno != 0)
//        return false;                          // パース失敗
//    msg.id = static_cast<uint32_t>(parsed_id); // ULONGからuint32_tへキャスト (サイズ注意)
//
//    if (seglist[2].length() > 3) {
//        msg.format = stmbed::CANFormat::CANExtended;
//    } else {
//        msg.format = stmbed::CANFormat::CANStandard;
//    }
//
//    // DLCのパース (例外を投げないstrtolを使用)
//    char *end_ptr_dlc;
//    errno = 0;
//    long parsed_dlc = strtol(seglist[3].c_str(), &end_ptr_dlc, 10);
//    if (*end_ptr_dlc != '\0' || errno != 0 || parsed_dlc < 0 || parsed_dlc > 8)
//        return false; // パース失敗 or 範囲外
//    msg.size = static_cast<uint8_t>(parsed_dlc);
//
//    // データバイトのパース (例外を投げないstrtoulを使用)
//    // RTRの判定: socketcandのプロトコルではDLCが0でデータがない場合や、
//    // 特定のフラグがあるかもしれない -> man socketcand で確認が必要
//    // ここでは、データ数がDLCと一致するかどうかでチェック
//    if (seglist.size() != (size_t)(4 + msg.size)) {
//        // データ数がDLCと合わない場合 -> RTRとみなすか、エラーとするか？
//        // socketcandの仕様を確認する必要あり。ここではエラーとする。
//        // (もしDLC>0なのにデータがないならパースエラー)
//        // (もしDLC=0でデータもないなら、それはそれでOKかもしれない)
//        if (msg.size > 0 || seglist.size() != 4) { // DLC>0なのにデータがない or DLC=0なのに余分なデータがある
//            return false;
//        }
//        // msg.rtr = true; // stmbed::CANMessage に rtr メンバがあれば設定
//    } else {
//        // msg.rtr = false; // rtr メンバがあれば設定
//    }
//
//    for (uint8_t i = 0; i < msg.size && (size_t)(4 + i) < seglist.size(); ++i) { // Wsign-compare 修正
//        char *end_ptr_data;
//        errno = 0;
//        unsigned long parsed_byte = strtoul(seglist[4 + i].c_str(), &end_ptr_data, 16);
//        if (*end_ptr_data != '\0' || errno != 0 || parsed_byte > 255)
//            return false; // パース失敗 or 範囲外
//        msg.data[i] = static_cast<uint8_t>(parsed_byte);
//    }
//
//    return true;
//}
//
//} // namespace protocol
//} // namespace socketcand
//
//class SocketcandClientChannel {
//public:
//    using TCPSocketType = netxduo::tcp_socket<512, 1024>;
//
//    SocketcandClientChannel(NX_IP *interface, const std::string &server_ip, uint16_t server_port,
//                            const std::string &target_interface, stmbed::CAN &hcan, int channel_id)
//        : nx_interface_(interface), server_ip_str_(server_ip), server_port_(server_port),
//          target_interface_name_(target_interface), can_(hcan), channel_id_(channel_id) {
//        // IPアドレス文字列をULONGに変換
//        server_ip_addr_ = parse_ipv4_string(server_ip_str_.c_str());
//        if (server_ip_addr_ == 0) {
//            printf("ERROR: Could not parse server IP '%s'\n", server_ip_str_.c_str());
//        } else {
//            printf("Server IP '%s' parsed to %lu.%lu.%lu.%lu (ULONG: 0x%lX)\n", server_ip_str_.c_str(),
//                   (server_ip_addr_ >> 24) & 0xFF, (server_ip_addr_ >> 16) & 0xFF, (server_ip_addr_ >> 8) & 0xFF,
//                   server_ip_addr_ & 0xFF, server_ip_addr_);
//        }
//    }
//
//    void start() {
//        main_thread_ = std::make_unique<threadx::static_thread<THREAD_STACK_SIZE>>(
//            "Main Thread", std::bind(&SocketcandClientChannel::channel_main, this, std::placeholders::_1));
//    }
//
//    int channel_id() const { return channel_id_; }
//
//private:
//    enum class State { DISCONNECTED, CONNECTING, CONNECTED, AUTHENTICATED };
//    volatile State current_state_ = State::DISCONNECTED;
//
//    void channel_main(ULONG thread_input) {
//        printf("Channel %d (%s): Main thread started.\n", channel_id_, target_interface_name_.c_str());
//        using namespace threadx;
//
//        can_.attach([&](const stmbed::CANMessage &msg) {
//            if (current_state_ == State::AUTHENTICATED) {
//                tx_msg_queue_.push(msg);
//            }
//        });
//
//        tcp_socket_ = std::make_unique<TCPSocketType>(nx_interface_);
//
//        while (1) {
//            if (current_state_ == State::DISCONNECTED) {
//                current_state_ = State::CONNECTING;
//                printf("Channel %d (%s): Attempting to connect to %s:%d...\n", channel_id_,
//                       target_interface_name_.c_str(), server_ip_str_.c_str(), server_port_);
//
//                if (server_ip_addr_ == 0) {
//                    printf("Channel %d (%s): Invalid server IP. Halting connect attempt.\n", channel_id_,
//                           target_interface_name_.c_str());
//                    this_thread::sleep_for(5000);
//                    current_state_ = State::DISCONNECTED;
//                    continue;
//                }
//
//                // ★★★★★ 修正箇所 ★★★★★
//                // connect前に明示的にバインドする (ポート0 = NX_ANY_PORT を指定)
//                printf("Channel %d (%s): Binding client socket to local port NX_ANY_PORT...\n", channel_id_,
//                       target_interface_name_.c_str());
//                // netxduo::tcp_socket ラッパーに bind メソッドがない場合、
//                // ネイティブのNetXDuo APIを直接呼ぶ必要があるかもしれません。
//                // ラッパーに get_native_handle() のようなメソッドがあると仮定します。
//                // もしラッパーに bind(port, timeout) があればそれを使います。
//                UINT bind_status =
//                    nx_tcp_client_socket_bind(&tcp_socket_->get_native_handle(), // ここはラッパーの実装依存
//                                              NX_ANY_PORT,                       // ポート0を指定
//                                              TX_NO_WAIT); // または適切な待機オプション
//                if (bind_status != NX_SUCCESS) {
//                    printf("Channel %d (%s): Failed to bind client socket (status=0x%02X). Retrying in 5 seconds...\n",
//                           channel_id_, target_interface_name_.c_str(), bind_status);
//                    // バインド失敗時の後処理 (必要ならソケット削除・再作成など)
//                    current_state_ = State::DISCONNECTED;
//                    this_thread::sleep_for(5000);
//                    continue; // ループの先頭に戻ってリトライ
//                }
//                printf("Channel %d (%s): Client socket bound successfully.\n", channel_id_,
//                       target_interface_name_.c_str());
//                // ★★★★★ 修正箇所ここまで ★★★★★
//
//                // 接続試行 (タイムアウト引数なし)
//                // connectメソッドが成功/失敗をどう示すか不明なため、try-connectパターンにする
//                // NetXDuoの nx_tcp_client_socket_connect を直接呼ぶ方が制御しやすい場合もある
//                tcp_socket_->connect(server_ip_addr_, server_port_);
//
//                // 接続成功したかどうかの確認方法が必要 (ラッパーの実装依存)
//                // ここでは、少し待ってから is_connected() で確認する例
//                this_thread::sleep_for(200); // 接続処理にある程度時間を与える
//
//                if (tcp_socket_->is_connected()) {
//                    printf("Channel %d (%s): TCP connection established.\n", channel_id_,
//                           target_interface_name_.c_str());
//                    current_state_ = State::CONNECTED;
//
//                    receive_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
//                        "Receive Thread",
//                        std::bind(&SocketcandClientChannel::receive_thread_entry, this, std::placeholders::_1));
//                    send_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
//                        "Send Thread",
//                        std::bind(&SocketcandClientChannel::send_thread_entry, this, std::placeholders::_1));
//
//                } else {
//                    printf("Channel %d (%s): Connection failed. Retrying in 5 seconds...\n", channel_id_,
//                           target_interface_name_.c_str());
//                    tcp_socket_->disconnect();
//                    current_state_ = State::DISCONNECTED;
//                    this_thread::sleep_for(5000);
//                }
//            } else {
//                if (current_state_ >= State::CONNECTED && !tcp_socket_->is_connected()) {
//                    printf("Channel %d (%s): Connection lost.\n", channel_id_, target_interface_name_.c_str());
//                    handle_disconnection();
//                }
//                this_thread::sleep_for(500);
//            }
//        }
//    }
//
//    void receive_thread_entry(ULONG thread_input) {
//        printf("Channel %d (%s): Receive thread started.\n", channel_id_, target_interface_name_.c_str());
//        using namespace threadx;
//
//        std::vector<uint8_t> recv_buf(512);
//        std::string line_buf;
//        line_buf.reserve(128);
//
//        while (current_state_ >= State::CONNECTED) {
//            size_t received_size = 0;
//            // データ受信 (タイムアウト引数なし)
//            bool success = tcp_socket_->receive(recv_buf.data(), received_size);
//
//            if (success && received_size > 0) {
//                line_buf.append(reinterpret_cast<char *>(recv_buf.data()), received_size);
//                size_t newline_pos;
//                while ((newline_pos = line_buf.find('\n')) != std::string::npos) {
//                    std::string line = line_buf.substr(0, newline_pos);
//                    line_buf.erase(0, newline_pos + 1);
//
//                    if (!line.empty() && line[0] == '<') {
//                        if (line.rfind("<frame ", 0) == 0) {
//                            stmbed::CANMessage msg;
//                            std::string received_interface;
//                            if (socketcand::protocol::parse_frame_command(line, received_interface, msg)) {
//                                if (received_interface == target_interface_name_) {
//                                    rx_msg_queue_.push(msg);
//                                } else { /* ignore */
//                                }
//                            } else { /* parse fail */
//                            }
//                        } else if (line.rfind("<ok ", 0) == 0) {
//                            printf("Socketcand OK: %s\n", line.c_str());
//                            if (line.find("open " + target_interface_name_) != std::string::npos &&
//                                current_state_ == State::CONNECTED) {
//                                printf("Channel %d (%s): Socketcand session authenticated.\n", channel_id_,
//                                       target_interface_name_.c_str());
//                                current_state_ = State::AUTHENTICATED;
//                            }
//                        } else if (line.rfind("<error ", 0) == 0) {
//                            printf("Socketcand ERROR: %s\n", line.c_str());
//                            if (line.find("open") != std::string::npos && current_state_ == State::CONNECTED) {
//                                printf("Channel %d (%s): Failed to open socketcand interface. Disconnecting.\n",
//                                       channel_id_, target_interface_name_.c_str());
//                                handle_disconnection(); // open失敗なら切断
//                                return;
//                            }
//                        } else { /* unknown */
//                        }
//                    }
//                }
//            } else if (!success) {
//                printf("Channel %d (%s): Receive error or connection closed.\n", channel_id_,
//                       target_interface_name_.c_str());
//                handle_disconnection();
//                break;
//            }
//
//            while (!rx_msg_queue_.empty() && can_.writeable()) {
//                can_.write(rx_msg_queue_.front());
//                rx_msg_queue_.pop();
//            }
//            this_thread::sleep_for(5);
//        }
//        printf("Channel %d (%s): Receive thread stopped.\n", channel_id_, target_interface_name_.c_str());
//    }
//
//    void send_thread_entry(ULONG thread_input) {
//        printf("Channel %d (%s): Send thread started.\n", channel_id_, target_interface_name_.c_str());
//        using namespace threadx;
//
//        std::string open_cmd = socketcand::protocol::format_open_command(target_interface_name_);
//        // sendの戻り値がvoidなので、成否チェックは削除。エラーは内部処理か別途検知を期待。
//        // キャストを追加
//        tcp_socket_->send(reinterpret_cast<uint8_t *>(const_cast<char *>(open_cmd.c_str())), open_cmd.length());
//        printf("Channel %d (%s): Sent %s", channel_id_, target_interface_name_.c_str(), open_cmd.c_str());
//
//        int wait_count = 0;
//        while (current_state_ != State::AUTHENTICATED && wait_count < 50) {
//            this_thread::sleep_for(100);
//            wait_count++;
//        }
//
//        if (current_state_ != State::AUTHENTICATED) {
//            printf("Channel %d (%s): Authentication timed out. Disconnecting.\n", channel_id_,
//                   target_interface_name_.c_str());
//            handle_disconnection();
//            return;
//        }
//
//        printf("Channel %d (%s): Authentication confirmed. Starting CAN frame transmission.\n", channel_id_,
//               target_interface_name_.c_str());
//
//        while (current_state_ == State::AUTHENTICATED) {
//            stmbed::CANMessage msg_to_send;
//            bool message_found = false;
//
//            // Mutexが必要なら有効化
//            // tx_msg_queue_mutex_.lock();
//            if (!tx_msg_queue_.empty()) {
//                msg_to_send = tx_msg_queue_.front();
//                tx_msg_queue_.pop();
//                message_found = true;
//            }
//            // tx_msg_queue_mutex_.unlock();
//
//            if (message_found) {
//                std::string send_cmd = socketcand::protocol::format_send_command(msg_to_send, target_interface_name_);
//                // sendの戻り値がvoidなので成否チェック削除。キャスト追加。
//                tcp_socket_->send(reinterpret_cast<uint8_t *>(const_cast<char *>(send_cmd.c_str())), send_cmd.length());
//            } else {
//                this_thread::sleep_for(5);
//            }
//        }
//        printf("Channel %d (%s): Send thread stopped.\n", channel_id_, target_interface_name_.c_str());
//
//        if (tcp_socket_->is_connected()) {
//            std::string close_cmd = socketcand::protocol::format_close_command(target_interface_name_);
//            // sendの戻り値がvoidなので成否チェック削除。キャスト追加。
//            tcp_socket_->send(reinterpret_cast<uint8_t *>(const_cast<char *>(close_cmd.c_str())), close_cmd.length());
//        }
//    }
//
//    void handle_disconnection(void) {
//        if (current_state_ != State::DISCONNECTED) {
//            printf("Channel %d (%s): Handling disconnection...\n", channel_id_, target_interface_name_.c_str());
//            current_state_ = State::DISCONNECTED;
//            receive_thread_.reset();
//            send_thread_.reset();
//            tcp_socket_->disconnect();
//            clear_queues();
//            printf("Channel %d (%s): Disconnected.\n", channel_id_, target_interface_name_.c_str());
//        }
//    }
//
//    void clear_queues() {
//        // tx_msg_queue_mutex_.lock();
//        std::queue<stmbed::CANMessage> empty_tx;
//        std::swap(tx_msg_queue_, empty_tx);
//        // tx_msg_queue_mutex_.unlock();
//
//        std::queue<stmbed::CANMessage> empty_rx;
//        std::swap(rx_msg_queue_, empty_rx);
//    }
//
//    // --- メンバ変数 ---
//    NX_IP *nx_interface_;
//    std::string server_ip_str_;
//    uint16_t server_port_;
//    ULONG server_ip_addr_ = 0;
//    std::string target_interface_name_;
//    stmbed::CAN &can_;
//    const int channel_id_;
//
//    constexpr static UINT THREAD_STACK_SIZE = 2048;
//    std::unique_ptr<threadx::static_thread<THREAD_STACK_SIZE>> main_thread_;
//    std::unique_ptr<threadx::static_thread<THREAD_STACK_SIZE>> receive_thread_;
//    std::unique_ptr<threadx::static_thread<THREAD_STACK_SIZE>> send_thread_;
//    std::unique_ptr<TCPSocketType> tcp_socket_;
//
//    std::queue<stmbed::CANMessage> tx_msg_queue_;
//    // threadx::mutex tx_msg_queue_mutex_; // 必要に応じて有効化
//    std::queue<stmbed::CANMessage> rx_msg_queue_;
//};
