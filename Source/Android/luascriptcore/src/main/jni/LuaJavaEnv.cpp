//
// Created by 冯鸿杰 on 16/9/29.
//

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <thread>
#include "LuaJavaEnv.h"
#include "LuaObjectManager.h"
#include "LuaObjectDescriptor.h"
#include "LuaJavaType.h"
#include "LuaJavaConverter.h"
#include "LuaValue.h"
#include "LuaContext.h"
#include "LuaDefine.h"
#include "LuaCoroutine.h"

/**
 * Java虚拟机对象
 */
static JavaVM *_javaVM;

/**
 * 附加线程标识
 */
static bool _attatedThread;

/**
 * JNI环境
 */
static JNIEnv *_env;

/**
 * JNI环境引用次数，每次获取环境时引用次数增加，reset时次数减少，直到为0时释放env
 */
static int _envRetainCount = 0;

/**
 * Java中上下文对象集合
 */
static std::map<jint, jobject> _javaObjectMap;

/**
 * 对象实例的lua引用对照表
 */
static std::map<int, cn::vimfung::luascriptcore::LuaObjectDescriptor*> _instanceMap;

/**
 * 环境引用表
 */
static std::map<std::thread::id, LuaJavaEnv *> _envRefs;

/**
 * Lua方法处理器
 *
 * @param context 上下文对象
 * @param methodName 方法名称
 * @param arguments 方法参数
 *
 * @returns 返回值
 */
static LuaValue* _luaMethodHandler (LuaContext *context, std::string methodName, LuaArgumentList arguments)
{
    JNIEnv *env = LuaJavaEnv::getEnv();
    LuaValue *retValue = NULL;

    jobject jcontext = LuaJavaEnv::getJavaLuaContext(env, context);
    if (jcontext != NULL)
    {
        if (env -> IsSameObject(jcontext, NULL) != JNI_TRUE)
        {
            static jclass contenxtClass = LuaJavaType::contextClass(env);
            static jmethodID invokeMethodID = env -> GetMethodID(contenxtClass, "methodInvoke", "(Ljava/lang/String;[Lcn/vimfung/luascriptcore/LuaValue;)Lcn/vimfung/luascriptcore/LuaValue;");
            static jclass luaValueClass = LuaJavaType::luaValueClass(env);

            jstring jMethodName = env -> NewStringUTF(methodName.c_str());

            //参数
            jobjectArray argumentArr = env -> NewObjectArray(arguments.size(), luaValueClass, NULL);
            int index = 0;
            for (LuaArgumentList::iterator it = arguments.begin(); it != arguments.end(); it ++)
            {
                LuaValue *argument = *it;
                jobject jArgument = LuaJavaConverter::convertToJavaLuaValueByLuaValue(env, context, argument);
                env -> SetObjectArrayElement(argumentArr, index, jArgument);
                env -> DeleteLocalRef(jArgument);
                index++;
            }

            jobject result = env -> CallObjectMethod(jcontext, invokeMethodID, jMethodName, argumentArr);
            if (result != NULL)
            {
                retValue = LuaJavaConverter::convertToLuaValueByJLuaValue(env, context, result);
                env -> DeleteLocalRef(result);
            }
            else
            {
                retValue = new LuaValue();
            }

            env -> DeleteLocalRef(argumentArr);
            env -> DeleteLocalRef(jMethodName);
        }
    }

    LuaJavaEnv::resetEnv(env);

    return retValue;
}

/**
 * Lua线程处理器
 *
 * @param context 上下文对象
 * @param methodName 方法名称
 * @param arguments 方法参数
 *
 * @returns 返回值
 */
static LuaValue* _luaThreadHandler (LuaContext *context, const std::string & methodName, LuaArgumentList arguments)
{
    JNIEnv *env = LuaJavaEnv::getEnv();
    LuaValue *retValue = NULL;

    jobject jcontext = LuaJavaEnv::getJavaLuaContext(env, context);
    if (jcontext != NULL)
    {
        if (env -> IsSameObject(jcontext, NULL) != JNI_TRUE)
        {
            static jclass contenxtClass = LuaJavaType::threadClass(env);
            static jmethodID invokeMethodID = env -> GetMethodID(contenxtClass, "methodInvoke", "(Ljava/lang/String;[Lcn/vimfung/luascriptcore/LuaValue;)Lcn/vimfung/luascriptcore/LuaValue;");
            static jclass luaValueClass = LuaJavaType::luaValueClass(env);

            jstring jMethodName = env -> NewStringUTF(methodName.c_str());

            //参数
            jobjectArray argumentArr = env -> NewObjectArray(arguments.size(), luaValueClass, NULL);
            int index = 0;
            for (LuaArgumentList::iterator it = arguments.begin(); it != arguments.end(); it ++)
            {
                LuaValue *argument = *it;
                jobject jArgument = LuaJavaConverter::convertToJavaLuaValueByLuaValue(env, context, argument);
                env -> SetObjectArrayElement(argumentArr, index, jArgument);
                env -> DeleteLocalRef(jArgument);
                index++;
            }

            jobject result = env -> CallObjectMethod(jcontext, invokeMethodID, jMethodName, argumentArr);
            if (result != NULL)
            {
                retValue = LuaJavaConverter::convertToLuaValueByJLuaValue(env, context, result);
                env -> DeleteLocalRef(result);
            }
            else
            {
                retValue = new LuaValue();
            }

            env -> DeleteLocalRef(argumentArr);
            env -> DeleteLocalRef(jMethodName);
        }
    }

    LuaJavaEnv::resetEnv(env);

    return retValue;
}


/**
 * Lua异常处理器
 */
static void _luaExceptionHandler (LuaContext *context, std::string const& message)
{
    JNIEnv *env = LuaJavaEnv::getEnv();

    jobject jcontext = LuaJavaEnv::getJavaLuaContext(env, context);
    if (jcontext != NULL)
    {
        jfieldID exceptHandlerFieldId = env -> GetFieldID(env -> GetObjectClass(jcontext), "_exceptionHandler", "Lcn/vimfung/luascriptcore/LuaExceptionHandler;");
        jobject exceptHandler = env -> GetObjectField(jcontext, exceptHandlerFieldId);
        if (exceptHandler != NULL)
        {
            jstring messageStr = env -> NewStringUTF(message.c_str());
            jmethodID onExceptMethodId = env -> GetMethodID(env -> GetObjectClass(exceptHandler), "onException", "(Ljava/lang/String;)V");
            env -> CallVoidMethod(exceptHandler, onExceptMethodId, messageStr);
            env -> DeleteLocalRef(messageStr);

            env -> DeleteLocalRef(exceptHandler);
        }
    }

    LuaJavaEnv::resetEnv(env);
}

static void _luaExportsNativeTypeHandler(LuaContext *context, std::string const& typeName)
{
    JNIEnv *env = LuaJavaEnv::getEnv();

    jobject jcontext = LuaJavaEnv::getJavaLuaContext(env, context);
    if (jcontext != NULL)
    {
        static jclass contenxtClass = LuaJavaType::contextClass(env);
        static jmethodID invokeMethodID = env -> GetMethodID(contenxtClass, "exportsNativeType", "(Ljava/lang/String;)V");

        jstring jTypeName = env -> NewStringUTF(typeName.c_str());

        env -> CallVoidMethod(jcontext, invokeMethodID, jTypeName);

        env -> DeleteLocalRef(jTypeName);
    }

    LuaJavaEnv::resetEnv(env);
}

void LuaJavaEnv::init(JavaVM *javaVM)
{
    _javaVM = javaVM;
    _attatedThread = false;
}

JNIEnv* LuaJavaEnv::getEnv()
{
    //fixed：解决多线程下获取JNIEnv对象时出现崩溃问题。之前的方法处理有误，没有根据线程来保留JNIEnv对象
    LuaJavaEnv *luaJavaEnv = NULL;
    std::thread::id tid = std::this_thread::get_id();

    auto it = _envRefs.find(tid);
    if (it == _envRefs.end())
    {
        //没有该线程的JNIEnv对象，开始获取对象

        JNIEnv *env = NULL;
        bool attachedThread = false;
        int status;

        status = _javaVM -> GetEnv((void **)&env, JNI_VERSION_1_4);

        if(status < 0)
        {
            status = _javaVM -> AttachCurrentThread(&env, NULL);
            if(status >= 0)
            {
                attachedThread = true;
            }
        }

        luaJavaEnv = new LuaJavaEnv();
        luaJavaEnv -> _jniEnv = env;
        luaJavaEnv -> _attachedThread = attachedThread;
        luaJavaEnv -> _count = 1;

        _envRefs[tid] = luaJavaEnv;
    }
    else
    {
        luaJavaEnv = it -> second;
        luaJavaEnv -> _count ++;
    }

    return luaJavaEnv -> _jniEnv;
}

void LuaJavaEnv::resetEnv(JNIEnv *env)
{
    //fixed:根据线程标识来释放对象
    std::thread::id tid = std::this_thread::get_id();
    auto it = _envRefs.find(tid);
    if (it != _envRefs.end())
    {
        LuaJavaEnv *luaJavaEnv = it -> second;
        luaJavaEnv -> _count --;
        if (luaJavaEnv -> _count <= 0)
        {
            //回收
            _envRefs.erase(it);

            if (luaJavaEnv -> _attachedThread)
            {
                _javaVM -> DetachCurrentThread();
            }

            luaJavaEnv -> release();
        }
    }
}

jobject LuaJavaEnv::createJavaLuaContext(JNIEnv *env, LuaContext *context)
{
    static jclass contextClass = LuaJavaType::contextClass(env);
    static jmethodID initMethodId = env -> GetMethodID(contextClass, "<init>", "(I)V");

    int nativeId = LuaObjectManager::SharedInstance() -> putObject(context);
    jobject jcontext = env -> NewObject(contextClass, initMethodId, nativeId);

    _javaObjectMap[context -> objectId()] = env -> NewWeakGlobalRef(jcontext);

    return jcontext;
}

jobject LuaJavaEnv::getJavaLuaContext(JNIEnv *env, LuaContext *context)
{
    std::map<jint, jobject>::iterator it =  _javaObjectMap.find(context->objectId());
    if (it != _javaObjectMap.end())
    {
        jobject jcontext = it -> second;
        if (env -> IsSameObject(jcontext, NULL) != JNI_TRUE)
        {
            return jcontext;
        }
        else
        {
            //如果Java中已释放对象则在集合中进行移除
            env -> DeleteWeakGlobalRef(jcontext);
            _javaObjectMap.erase(it);
        }
    }

    return NULL;
}

void LuaJavaEnv::associcateInstance(JNIEnv *env, jobject instance, cn::vimfung::luascriptcore::LuaObjectDescriptor *descriptor)
{
    if (env -> IsInstanceOf(instance, LuaJavaType::luaBaseObjectClass(env)) == JNI_TRUE)
    {
        //设置instance的nativeId为LuaObjectDescriptor的objectId
        jfieldID nativeFieldId = env -> GetFieldID(env -> GetObjectClass(instance), "_nativeId", "I");
        env -> SetIntField(instance, nativeFieldId, descriptor -> objectId());

        std::map<int, LuaObjectDescriptor*>::iterator it =  _instanceMap.find(descriptor -> objectId());
        if (it == _instanceMap.end())
        {
            _instanceMap[descriptor -> objectId()] = descriptor;
        }
    }
}

void LuaJavaEnv::removeAssociateInstance(JNIEnv *env, jobject instance)
{
    if (env -> IsInstanceOf(instance, LuaJavaType::luaBaseObjectClass(env)) == JNI_TRUE)
    {
        jfieldID nativeFieldId = env->GetFieldID(env->GetObjectClass(instance), "_nativeId", "I");
        jint nativeId = env->GetIntField(instance, nativeFieldId);

        std::map<int, LuaObjectDescriptor*>::iterator it =  _instanceMap.find(nativeId);
        if (it != _instanceMap.end())
        {
            _instanceMap.erase(it);
        }
    }
}

LuaObjectDescriptor* LuaJavaEnv::getAssociateInstanceRef(JNIEnv *env, jobject instance)
{
    LuaObjectDescriptor *objectDescriptor = NULL;

    if (instance != NULL && env -> IsInstanceOf(instance, LuaJavaType::luaBaseObjectClass(env)) == JNI_TRUE)
    {
        jfieldID nativeFieldId = env -> GetFieldID(env -> GetObjectClass(instance), "_nativeId", "I");
        jint nativeId = env -> GetIntField(instance, nativeFieldId);

        std::map<int, LuaObjectDescriptor*>::iterator it =  _instanceMap.find(nativeId);
        if (it != _instanceMap.end())
        {
            objectDescriptor = it -> second;
        }

    }

    return objectDescriptor;
}

void LuaJavaEnv::releaseObject(JNIEnv *env, jint objectId)
{
    std::map<jint, jobject>::iterator it =  _javaObjectMap.find(objectId);
    if (it != _javaObjectMap.end())
    {
        //为LuaContext对象作处理,解除对象引用
        env -> DeleteWeakGlobalRef(it -> second);
        _javaObjectMap.erase(it);
    }

    LuaObjectManager::SharedInstance() -> removeObject(objectId);
}

LuaMethodHandler LuaJavaEnv::luaMethodHandler()
{
    return (LuaMethodHandler) _luaMethodHandler;
}

std::string LuaJavaEnv::getJavaClassNameByInstance(JNIEnv *env, jobject instance)
{
    std::string className;
    if (env -> IsInstanceOf(instance, LuaJavaType::luaObjectClass(env)) == JNI_TRUE)
    {
        jmethodID getModuleNameMethodId = env -> GetStaticMethodID(LuaJavaType::moduleClass(env), "_getModuleName", "(Ljava/lang/Class;)Ljava/lang/String;");
        jstring jclassName = (jstring)env -> CallStaticObjectMethod(LuaJavaType::moduleClass(env), getModuleNameMethodId, env -> GetObjectClass(instance));
        if (jclassName != NULL)
        {
            const char *classNameCStr = env -> GetStringUTFChars(jclassName, NULL);
            className = classNameCStr;
            env -> ReleaseStringUTFChars(jclassName, classNameCStr);
            env -> DeleteLocalRef(jclassName);
        }
    }

    return className;
}

LuaExportsNativeTypeHandler LuaJavaEnv::getExportsNativeTypeHandler()
{
    return _luaExportsNativeTypeHandler;
}

cn::vimfung::luascriptcore::LuaExceptionHandler LuaJavaEnv::getExceptionHandler()
{
    return _luaExceptionHandler;
}

std::string LuaJavaEnv::getJavaClassName(JNIEnv *env, jclass cls, bool simpleName)
{
    // Get the class object's class descriptor
    jclass clsClazz = env->GetObjectClass(cls);

    // Find the getSimpleName() method in the class object
    jmethodID methodId = NULL;
    if (simpleName)
    {
        methodId = env->GetMethodID(clsClazz, "getSimpleName", "()Ljava/lang/String;");
    }
    else
    {
        methodId = env->GetMethodID(clsClazz, "getName", "()Ljava/lang/String;");
    }

    jstring className = (jstring) env->CallObjectMethod(cls, methodId);

    std::string name;

    const char *nameCStr = env -> GetStringUTFChars(className, NULL);
    name = nameCStr;
    env -> ReleaseStringUTFChars(className, nameCStr);
    env -> DeleteLocalRef(className);

    return name;
}

jobject LuaJavaEnv::getExportTypeManager(JNIEnv *env)
{
    //fixed: 修复直接返回CallStaticObjectMethod的返回值导致临时变量表溢出问题
    static jobject exportTypeManager = NULL;
    if (exportTypeManager == NULL)
    {
        jclass exportTypeManagerCls = LuaJavaType::exportTypeManagerClass(env);

        jmethodID defaultManagerMethodId = env -> GetStaticMethodID(exportTypeManagerCls, "getDefaultManager", "()Lcn/vimfung/luascriptcore/LuaExportTypeManager;");
        jobject manager = env -> CallStaticObjectMethod(exportTypeManagerCls, defaultManagerMethodId);
        exportTypeManager = env -> NewWeakGlobalRef(manager);
        env -> DeleteLocalRef(manager);
    }

    return exportTypeManager;

}

jclass LuaJavaEnv::findClass(JNIEnv *env, std::string className)
{
    return env -> FindClass(className.c_str());
}