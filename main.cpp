#include "Log.hpp"

#include "SocketCAN.hpp"

class CanReceiver final : public ICanObserver
{
public:
    void onCanFrameReceived(const CanFrame& frame) override
    {
        LOGD("RX ID=0x%X TYPE=%s LEN=%u DATA=",
            frame.mCanId,
            frame.isCanFD() ? "FD" : "CLASSIC",
            frame.mLen);

        LOG_Dump(LOG_LEVEL_DEBUG, frame.mData, frame.mLen);
    }
};

int main()
{
    SocketCAN can0;

    CanReceiver listener;

    if (!can0.open("can0", false))
        return -1;

    can0.addListener(ICanInterface::ANY_CAN_ID, &listener);
    //can0.addListener(0x80210101, &listener);

    while (1) { sleep(1); }

    can0.close();

    return 0;
}
