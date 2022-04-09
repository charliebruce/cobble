#include "../../cobble.h"
#include "../../cobble_events.h"

#include <jni.h>
#include <android/log.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

CobbleStatus status = Uninitialised;
CobbleErrorCode error_code = NoError;

CobbleStatus cobble_status(void) {
    return status;
}

CobbleErrorCode cobble_error_get(void) {
    return error_code;
}

static JNIEnv* env = NULL;
static JavaVM* gJVM;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJVM = vm;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Failed to get the JVM environment");
    }

    jint ver = (*env)->GetVersion(env);

    __android_log_print(ANDROID_LOG_INFO, "TRACKERS", "JVM load succeeded, version %i.%i", ((ver >> 16) & 0x0F), (ver & 0x0F));

    return JNI_VERSION_1_6;
}

jclass _GetImpl() {
    jclass impl = (*env)->FindClass(env, "com/cjb248/cobble/AndroidBLEImpl");
    if (impl == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Class \"AndroidBLEImpl\" not found");
    }
    return impl;
}


void call_static_void_function(char* name) {
    jclass cls = _GetImpl();
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, name, "()V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void %s()\" not found", name);
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid);
    }
}

void cobble_read(const char* characteristic_uuid) {

    jstring jstr_characteristic_uuid = (*env)->NewStringUTF(env, characteristic_uuid);

    jclass cls = _GetImpl();
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "cobble_read", "(Ljava/lang/String;)V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void cobble_read(String)\" not found");
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid, jstr_characteristic_uuid);
    }
}

void cobble_write(const char* characteristic_uuid, uint8_t *data, int len) {

    jbyteArray jarr_data = (*env)->NewByteArray(env, len);
    void *temp = (*env)->GetPrimitiveArrayCritical(env, (jarray)jarr_data, 0);
    memcpy(temp, data, len);
    (*env)->ReleasePrimitiveArrayCritical(env, jarr_data, temp, 0);
    jstring jstr_characteristic_uuid = (*env)->NewStringUTF(env, characteristic_uuid);

    jclass cls = _GetImpl();
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "cobble_write", "(Ljava/lang/String;[B)V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void cobble_write(String, byte[])\" not found");
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid, jstr_characteristic_uuid, jarr_data);
    }
}

void cobble_connect(const char* identifier) {

    jstring jstr = (*env)->NewStringUTF(env, identifier);

    jclass cls = _GetImpl();
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "cobble_connect", "(Ljava/lang/String;)V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void cobble_connect(String)\" not found");
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid, jstr);
    }
}

void cobble_disconnect(void) {
    call_static_void_function("cobble_disconnect");
}

void cobble_init(void) {
    call_static_void_function("cobble_init");
}

void cobble_deinit(void) {
    call_static_void_function("cobble_deinit");
}

void cobble_scan_stop(void) {
    call_static_void_function("cobble_scan_stop");
}

void cobble_scan_start(const char* service_uuids) {

    jclass cls = _GetImpl();
    jstring jstr = (*env)->NewStringUTF(env, service_uuids);

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "cobble_scan_start", "(Ljava/lang/String;)V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void cobble_scan_start(String)\" not found");
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid, jstr);
    }
    
}

void cobble_subscribe(const char* characteristic) {

    jclass cls = _GetImpl();
    jstring jstr = (*env)->NewStringUTF(env, characteristic);

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "cobble_subscribe", "(Ljava/lang/String;)V");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Method \"void cobble_subscribe(String)\" not found");
    } else {
        (*env)->CallStaticVoidMethod(env, cls, mid, jstr);
    }

}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_SetStatus(JNIEnv* env, jobject obj, jint newStatus) {

    // We handle it this way to avoid needing duplicate definitions of CobbleStatus and CobbleErrorCode between C and Java
    switch(newStatus) {
        case 0:
            status = Uninitialised;
            break;
        case 1:
            status = Initialised;
            break;
        case 2:
            status = Scanning;
            break;
        case 3:
            status = Connecting;
            break;
        case 4:
            status = Connected;
            break;
        default:
            status = CobbleError; // Unhandled error code - Java code has a value the native code doesn't
            break;
    }
    status = newStatus;
}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_SetError(JNIEnv* env, jobject obj, jint newError) {

    status = CobbleError; // Setting the error code implies our base status should also be an error

    switch(newError) {
        case 0:
            error_code = NoError;
            break;
        case 1:
            error_code = HardwareUnsupported;
            break;
        case 2:
            error_code = HardwareTurnedOff;
            break;
        case 3:
            error_code = PermissionsNotGranted;
            break;
        default:
            error_code = UnknownError;
            break;
    }


}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_scanresult(JNIEnv* env, jobject obj, jstring name, jint rssi, jstring identifier) {
    char* nativeName = (char*)((*env)->GetStringUTFChars(env, name, 0));
    char* nativeIdent = (char*)((*env)->GetStringUTFChars(env, identifier, 0));
    cobble_event_scanresult(nativeName, (int) rssi, nativeIdent);
    (*env)->ReleaseStringUTFChars(env, name, nativeName);
    (*env)->ReleaseStringUTFChars(env, identifier, nativeIdent);
}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_Connected(JNIEnv* env, jobject obj, jstring str) {

    char* nativeString = (char*)((*env)->GetStringUTFChars(env, str, 0));

    cobble_event_connectionstatus(nativeString, ConnectionStatus_DidConnect);

    (*env)->ReleaseStringUTFChars(env, str, nativeString);

}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_Disconnected(JNIEnv* env, jobject obj, jstring str) {

    char* nativeString = (char*)((*env)->GetStringUTFChars(env, str, 0));

    cobble_event_connectionstatus(nativeString, ConnectionStatus_DidDisconnect);

    (*env)->ReleaseStringUTFChars(env, str, nativeString);

}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_ConnectError(JNIEnv* env, jobject obj, jstring str) {

    char* nativeString = (char*)((*env)->GetStringUTFChars(env, str, 0));

    cobble_event_connectionstatus(nativeString, ConnectionStatus_DidConnectFailed);
    
    (*env)->ReleaseStringUTFChars(env, str, nativeString);

}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_characteristicupdate(JNIEnv* env, jobject obj, jstring str, jbyteArray j_arr) {

    char* uuid = (char*)((*env)->GetStringUTFChars(env, str, 0));
    jbyte* buffer = (*env)->GetByteArrayElements(env, j_arr, NULL);
    jint num_bytes = (*env)->GetArrayLength(env, j_arr);
    cobble_event_updatevalue(uuid, (const uint8_t*) buffer, (int) num_bytes);
    (*env)->ReleaseByteArrayElements(env, j_arr, buffer, JNI_ABORT);
    (*env)->ReleaseStringUTFChars(env, str, uuid);

}

JNIEXPORT void JNICALL Java_com_cjb248_cobble_AndroidBLEImpl_characteristicdiscovered(JNIEnv* env, jobject obj, jstring j_svc_uuid, jstring j_char_uuid) {

    char* svc_uuid = (char*)((*env)->GetStringUTFChars(env, j_svc_uuid, 0));
    char* char_uuid = (char*)((*env)->GetStringUTFChars(env, j_char_uuid, 0));

    cobble_event_characteristicdiscovered(svc_uuid, char_uuid);

    (*env)->ReleaseStringUTFChars(env, j_svc_uuid, svc_uuid);
    (*env)->ReleaseStringUTFChars(env, j_char_uuid, char_uuid);

}

