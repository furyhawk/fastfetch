#include "publicip.h"
#include "common/networking.h"

#define FF_UNITIALIZED ((const char*)(uintptr_t) -1)
static FFNetworkingState state;
static const char* status = FF_UNITIALIZED;

void ffPreparePublicIp(FFPublicIpOptions* options)
{
    if (status != FF_UNITIALIZED)
    {
        fputs("Error: this module can only be used once due to internal limitations\n", stderr);
        exit(1);
    }

    if (options->url.length == 0)
        status = ffNetworkingSendHttpRequest(&state, "ipinfo.io", "/json", NULL);
    else
    {
        FF_STRBUF_AUTO_DESTROY host = ffStrbufCreateCopy(&options->url);
        ffStrbufSubstrAfterFirstS(&host, "://");
        uint32_t pathStartIndex = ffStrbufFirstIndexC(&host, '/');

        FF_STRBUF_AUTO_DESTROY path = ffStrbufCreate();
        if(pathStartIndex != host.length)
        {
            ffStrbufAppendNS(&path, pathStartIndex, host.chars + (host.length - pathStartIndex));
            host.length = pathStartIndex;
            host.chars[pathStartIndex] = '\0';
        }

        status = ffNetworkingSendHttpRequest(&state, host.chars, path.length == 0 ? "/" : path.chars, NULL);
    }
}

static inline void wrapYyjsonFree(yyjson_doc** doc)
{
    assert(doc);
    if (*doc)
        yyjson_doc_free(*doc);
}

const char* ffDetectPublicIp(FFPublicIpOptions* options, FFPublicIpResult* result)
{
    if (status == FF_UNITIALIZED)
        ffPreparePublicIp(options);

    if (status != NULL)
        return status;

    FF_STRBUF_AUTO_DESTROY response = ffStrbufCreateA(4096);
    const char* error = ffNetworkingRecvHttpResponse(&state, &response, options->timeout);
    if (error == NULL)
        ffStrbufSubstrAfterFirstS(&response, "\r\n\r\n");
    else
        return error;

    if (response.length == 0)
        return "Empty server response received";

    if (options->url.length == 0)
    {
        yyjson_doc* __attribute__((__cleanup__(wrapYyjsonFree))) doc = yyjson_read_opts(response.chars, response.length, 0, NULL, NULL);
        if (doc)
        {
            yyjson_val* root = yyjson_doc_get_root(doc);
            ffStrbufAppendS(&result->ip, yyjson_get_str(yyjson_obj_get(root, "ip")));
            ffStrbufDestroy(&result->location);
            ffStrbufInitF(&result->location, "%s, %s", yyjson_get_str(yyjson_obj_get(root, "city")), yyjson_get_str(yyjson_obj_get(root, "country")));
            return NULL;
        }
    }

    ffStrbufDestroy(&result->ip);
    ffStrbufInitMove(&result->ip, &response);
    ffStrbufTrimRightSpace(&result->ip);
    return NULL;
}
