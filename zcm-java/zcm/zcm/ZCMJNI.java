package zcm.zcm;

class ZCMJNI
{
    static {
        System.loadLibrary("zcmjni");
    }

    private long nativePtr = 0;
    private native void initializeNative();
    public ZCMJNI() { initializeNative(); }

    // This method publishes the provided data on the requested channel
    public native int publish(String channel, byte[] data, int offset, int length);

    // This method registers the ZCM object for an upcall to receiveMessage()
    public native int subscribe(String channel, ZCM zcm);
}
