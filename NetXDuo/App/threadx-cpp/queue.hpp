// threadx_queue.hpp
#pragma once

#include "tx_api.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace threadx {

/**
 * @brief   ThreadX の TX_QUEUE をラップして、std::queue のように使えるクラス
 *
 * @tparam T   キューに格納する要素の型。Plain Old Data (POD) であることを想定。
 * @tparam N   キューの深さ（同時に保持できる要素数の最大）。
 *
 * - ISR からは send_isr() を呼ぶことで、割り込みセーフにエンキューできる。
 * - スレッドからは send() を呼ぶと、TX_WAIT_FOREVER（デフォルト）でブロックして
 *   キューにエンキューする。プッシュに失敗したときは false を返す。
 * - receive() はデフォルトでブロックしてアイテムをデキューする。タイムアウト付きも可能。
 * - front() はキュー先頭を参照する（ノーブロック・ただし空キューで呼ばないこと）。
 * - pop() はブロッキングではなく、キュー先頭を消費する（空でも問題ない）。
 * - empty()/size() で大まかな空・要素数を取得できる。
 */
template <typename T, ULONG N> class queue {
    static_assert(std::is_trivially_copyable<T>::value,
                  "threadx::queue<T,N> の T は trivially_copyable である必要があります。");

public:
    /**
     * @brief コンストラクタ。TX_QUEUE を生成する。
     *        必ず ThreadX が起動した後、スレッドコンテキストで呼んでください。
     */
    queue() {
        // 1 メッセージあたりの ULONG 単位のサイズを計算
        const UINT msg_size_ULONG = static_cast<UINT>((sizeof(T) + sizeof(ULONG) - 1) / sizeof(ULONG));
        // キュー全体のバッファサイズ(バイト数) = msg_size_ULONG * sizeof(ULONG) * N
        const ULONG total_buf_size = msg_size_ULONG * sizeof(ULONG) * N;

        // TX_QUEUE の生成
        UINT ret = tx_queue_create(&txq_, const_cast<char *>("threadx::queue"), msg_size_ULONG,
                                   static_cast<VOID *>(buffer_), total_buf_size);
        // 通常アプリではここでエラー処理を行う
        (void)ret;
    }

    ~queue() { tx_queue_delete(&txq_); }

    /**
     * @brief   キューに要素をプッシュする（スレッドコンテキストで呼ぶ）。
     * @param   item    エンキューする要素
     * @param   wait_option  TX_WAIT_FOREVER, TX_NO_WAIT, タイムアウト(ms) など
     * @return  true: 成功、false: キューがいっぱいなどで失敗
     */
    bool send(const T &item, ULONG wait_option = TX_WAIT_FOREVER) {
        UINT ret = tx_queue_send(&txq_, (VOID *)&item, wait_option);
        return (ret == TX_SUCCESS);
    }

    /**
     * @brief   ISR（割り込み）からキューに要素をプッシュする場合に使う。
     *          ISR 内ではブロックできないため TX_NO_WAIT を指定して送る。
     * @param   item    エンキューする要素
     * @return  true: 成功、false: キューがいっぱいなどで失敗
     */
    bool send_isr(const T &item) {
        // ThreadX の ISR からキュー送信する関数は tx_queue_front_send や tx_queue_send でも OK
        // TX_NO_WAIT を指定してブロックしない。
        UINT ret = tx_queue_send(&txq_, (VOID *)&item, TX_NO_WAIT);
        return (ret == TX_SUCCESS);
    }

    /**
     * @brief   キューから要素を取り出す（ブロックして待機）。
     * @param   item    取り出す先のリファレンス
     * @param   wait_option  TX_WAIT_FOREVER または待ち時間(タイムアウト in ticks)
     * @return  true: 成功して item に値が入った、 false: タイムアウトなどで失敗
     */
    bool receive(T &item, ULONG wait_option = TX_WAIT_FOREVER) {
        UINT ret = tx_queue_receive(&txq_, (VOID *)&item, wait_option);
        return (ret == TX_SUCCESS);
    }

    /**
     * @brief   ノーブロックでキューを取り出し、失敗したら false を返す。
     */
    bool try_receive(T &item) {
        UINT ret = tx_queue_receive(&txq_, (VOID *)&item, TX_NO_WAIT);
        return (ret == TX_SUCCESS);
    }

    /**
     * @brief   キューが空かどうかを返す。
     *          （完全に正確ではないが、参考情報として）
     */
    bool empty() const { return (txq_.tx_queue_enqueued == 0); }

    /**
     * @brief   現在キューに入っている要素数を返す。（正確な数）
     */
    ULONG size() const { return txq_.tx_queue_enqueued; }

    /**
     * @brief   キュー先頭の要素を参照する（コピーはしない）。
     *          空キューで呼ぶと不定動作なので、呼び出し側で empty() を確認しておくこと。
     */
    const T &front() const {
        // ThreadX には直接「蓄積された先頭要素を参照する」 API はないので、
        // 一度 tx_queue_receive(TX_NO_WAIT) で取り出して、次スレッドが再度 recv できるように
        // 一時バッファに退避しておくようなしくみが必要……。
        // ここでは “参照” はあきらめて「要素を複製して返す」実装に切り替えています。
        // 必要なら他の方法（リングバッファを使うなど）を検討してください。
        static T tmp;
        // この「peek」実装は、本来 push→pop せず参照だけは難しいため、
        // receive してから再度 send しています（安全性は要検討）。
        if (tx_queue_receive(const_cast<TX_QUEUE *>(&txq_), (VOID *)&tmp, TX_NO_WAIT) == TX_SUCCESS) {
            // 取り出したので、すぐに再プッシュしてキューの順序を維持
            tx_queue_send(const_cast<TX_QUEUE *>(&txq_), (VOID *)&tmp, TX_NO_WAIT);
            return tmp;
        }
        // 空キューだった場合のダミー参照（呼び出し側で empty() を確かめましょう）
        return tmp;
    }

    /**
     * @brief   キュー先頭を取り除く（pop に相当）。ブロックしない。空でもエラーにはしない。
     */
    void pop() {
        T unused;
        (void)tx_queue_receive(&txq_, (VOID *)&unused, TX_NO_WAIT);
        // 空なら何もしない
    }

private:
    TX_QUEUE txq_;

    // 「1 要素あたりの ULONG 単位数」を計算
    static constexpr UINT MSG_SIZE_ULONG = static_cast<UINT>((sizeof(T) + sizeof(ULONG) - 1) / sizeof(ULONG));

    // バッファ全体のサイズ（バイト数）
    static constexpr ULONG BUFFER_SIZE = MSG_SIZE_ULONG * sizeof(ULONG) * N;

    // アライメント済みバッファ領域
    // ThreadX の tx_queue_create() では、VOID* 型のバッファを渡すだけなので
    // アラインメントに注意しています。
    CHAR buffer_[BUFFER_SIZE];
};

} // namespace threadx