// THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY
// BY HAND!!
//
// Generated by zcm-gen

#include <stdint.h>
#include <stdlib.h>
#include <zcm/zcm_coretypes.h>
#include <zcm/zcm.h>

#ifndef _array_multidim1_h
#define _array_multidim1_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _array_multidim1 array_multidim1;
struct _array_multidim1
{
    double     arr1[2][3][4][5];
    int32_t    i1;
    int16_t    i2;
    double     ****arr2;
    double     ****arr3;
};

/**
 * Create a deep copy of a array_multidim1.
 * When no longer needed, destroy it with array_multidim1_destroy()
 */
array_multidim1* array_multidim1_copy(const array_multidim1* to_copy);

/**
 * Destroy an instance of array_multidim1 created by array_multidim1_copy()
 */
void array_multidim1_destroy(array_multidim1* to_destroy);

/**
 * Identifies a single subscription.  This is an opaque data type.
 */
typedef struct _array_multidim1_subscription_t array_multidim1_subscription_t;

/**
 * Prototype for a callback function invoked when a message of type
 * array_multidim1 is received.
 */
typedef void(*array_multidim1_handler_t)(const zcm_recv_buf_t *rbuf,
             const char *channel, const array_multidim1 *msg, void *userdata);

/**
 * Publish a message of type array_multidim1 using ZCM.
 *
 * @param zcm The ZCM instance to publish with.
 * @param channel The channel to publish on.
 * @param msg The message to publish.
 * @return 0 on success, <0 on error.  Success means ZCM has transferred
 * responsibility of the message data to the OS.
 */
int array_multidim1_publish(zcm_t *zcm, const char *channel, const array_multidim1 *msg);

/**
 * Subscribe to messages of type array_multidim1 using ZCM.
 *
 * @param zcm The ZCM instance to subscribe with.
 * @param channel The channel to subscribe to.
 * @param handler The callback function invoked by ZCM when a message is received.
 *                This function is invoked by ZCM during calls to zcm_handle() and
 *                zcm_handle_timeout().
 * @param userdata An opaque pointer passed to @p handler when it is invoked.
 * @return pointer to subscription type, NULL if failure. Must clean up
 *         dynamic memory by passing the pointer to array_multidim1_unsubscribe.
 */
array_multidim1_subscription_t* array_multidim1_subscribe(zcm_t *zcm, const char *channel, array_multidim1_handler_t handler, void *userdata);

/**
 * Removes and destroys a subscription created by array_multidim1_subscribe()
 */
int array_multidim1_unsubscribe(zcm_t *zcm, array_multidim1_subscription_t* hid);
/**
 * Encode a message of type array_multidim1 into binary form.
 *
 * @param buf The output buffer.
 * @param offset Encoding starts at this byte offset into @p buf.
 * @param maxlen Maximum number of bytes to write.  This should generally
 *               be equal to array_multidim1_encoded_size().
 * @param msg The message to encode.
 * @return The number of bytes encoded, or <0 if an error occured.
 */
int array_multidim1_encode(void *buf, int offset, int maxlen, const array_multidim1 *p);

/**
 * Decode a message of type array_multidim1 from binary form.
 * When decoding messages containing strings or variable-length arrays, this
 * function may allocate memory.  When finished with the decoded message,
 * release allocated resources with array_multidim1_decode_cleanup().
 *
 * @param buf The buffer containing the encoded message
 * @param offset The byte offset into @p buf where the encoded message starts.
 * @param maxlen The maximum number of bytes to read while decoding.
 * @param msg Output parameter where the decoded message is stored
 * @return The number of bytes decoded, or <0 if an error occured.
 */
int array_multidim1_decode(const void *buf, int offset, int maxlen, array_multidim1 *msg);

/**
 * Release resources allocated by array_multidim1_decode()
 * @return 0
 */
int array_multidim1_decode_cleanup(array_multidim1 *p);

/**
 * Check how many bytes are required to encode a message of type array_multidim1
 */
int array_multidim1_encoded_size(const array_multidim1 *p);

// ZCM support functions. Users should not call these
int64_t __array_multidim1_get_hash(void);
uint64_t __array_multidim1_hash_recursive(const __zcm_hash_ptr *p);
int     __array_multidim1_encode_array(void *buf, int offset, int maxlen, const array_multidim1 *p, int elements);
int     __array_multidim1_decode_array(const void *buf, int offset, int maxlen, array_multidim1 *p, int elements);
int     __array_multidim1_decode_array_cleanup(array_multidim1 *p, int elements);
int     __array_multidim1_encoded_array_size(const array_multidim1 *p, int elements);
int     __array_multidim1_clone_array(const array_multidim1 *p, array_multidim1 *q, int elements);

#ifdef __cplusplus
}
#endif

#endif
