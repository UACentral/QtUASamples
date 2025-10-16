#include <open62541/client_config_default.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/log_stdout.h>

// #include "common.h"

#include <signal.h>
#include <stdlib.h>

/* sleep_ms */
#ifdef UA_ARCHITECTURE_WIN32
# include <synchapi.h>
# define sleep_ms(ms) Sleep(ms)
#else
# include <unistd.h>
# define sleep_ms(ms) usleep(ms * 1000)
#endif

static UA_Boolean running = true;

static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received Ctrl-C");
    running = 0;
}

static void
handler_currentTimeChanged(UA_Client *client, UA_UInt32 subId, void *subContext,
                           UA_UInt32 monId, void *monContext, UA_DataValue *value) {
    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double val = *(UA_Double *) value->value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "New value: %f", val);
    } else if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_FLOAT])) {
        UA_Float val = *(UA_Float *) value->value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "New value: %f", val);
    } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Value type: %s", value->value.type->typeName);
    }
}

static void
deleteSubscriptionCallback(UA_Client *client, UA_UInt32 subscriptionId, void *subscriptionContext) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Subscription Id %u was deleted", subscriptionId);
}

static void
subscriptionInactivityCallback (UA_Client *client, UA_UInt32 subId, void *subContext) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Inactivity for subscription %u", subId);
}

static void
monCallback(UA_Client *client, void *userdata,
            UA_UInt32 requestId, UA_CreateMonitoredItemsResponse *r) {
    if(0 < r->resultsSize && r->results[0].statusCode == UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Monitoring ns=2;s=0:TEST1/SGGN1/OUT.CV, id %u",
                    r->results[0].monitoredItemId);
    }
}

static void
createSubscriptionCallback(UA_Client *client, void *userdata,
                           UA_UInt32 requestId, UA_CreateSubscriptionResponse *r) {
    if (r->subscriptionId == 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "response->subscriptionId == 0, %u", r->subscriptionId);
    } else if (r->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Create subscription failed, serviceResult %u",
                    r->responseHeader.serviceResult);
    } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Create subscription succeeded, id %u", r->subscriptionId);

        /* Add a MonitoredItem */
        UA_NodeId nodeToMonitor =
            UA_NODEID_STRING(2, "0:TEST1/SGGN1/OUT.CV");
        UA_CreateMonitoredItemsRequest req;
        UA_CreateMonitoredItemsRequest_init(&req);
        UA_MonitoredItemCreateRequest monRequest =
            UA_MonitoredItemCreateRequest_default(nodeToMonitor);
        req.itemsToCreate = &monRequest;
        req.itemsToCreateSize = 1;
        req.subscriptionId = r->subscriptionId;

        UA_Client_DataChangeNotificationCallback dataChangeNotificationCallback[1] = { handler_currentTimeChanged };
        UA_StatusCode retval =
            UA_Client_MonitoredItems_createDataChanges_async(client, req, NULL,
                                                             dataChangeNotificationCallback, NULL,
                                                             monCallback, NULL, NULL);
        if (retval != UA_STATUSCODE_GOOD)
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "UA_Client_MonitoredItems_createDataChanges_async ", UA_StatusCode_name(retval));
    }
}

static void
stateCallback(UA_Client *client, UA_SecureChannelState channelState,
              UA_SessionState sessionState, UA_StatusCode recoveryStatus) {
    switch(channelState) {
    case UA_SECURECHANNELSTATE_CLOSED:
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "The client is disconnected");
        break;
    case UA_SECURECHANNELSTATE_HEL_SENT:
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for ack");
        break;
    case UA_SECURECHANNELSTATE_OPN_SENT:
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for OPN Response");
        break;
    case UA_SECURECHANNELSTATE_OPEN:
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A SecureChannel to the server is open");
        break;
    default:
        break;
    }

    switch(sessionState) {
    case UA_SESSIONSTATE_ACTIVATED: {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A session with the server is activated");
        /* A new session was created. We need to create the subscription. */
        /* Create a subscription */
        UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
        UA_StatusCode retval =
            UA_Client_Subscriptions_create_async(client, request, NULL, NULL, deleteSubscriptionCallback,
                                                 createSubscriptionCallback, NULL, NULL);
        if (retval != UA_STATUSCODE_GOOD)
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "UA_Client_Subscriptions_create_async ", UA_StatusCode_name(retval));
        }
        break;
    case UA_SESSIONSTATE_CLOSED:
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session disconnected");
        break;
    default:
        break;
    }
}

int
main(void) {
    signal(SIGINT, stopHandler); /* catches ctrl-c */

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *cc = UA_Client_getConfig(client);
    // UA_LogLevel log_level = UA_LOGLEVEL_ERROR;
    UA_LogLevel log_level = UA_LOGLEVEL_TRACE;
    UA_Logger logger = UA_Log_Stdout_withLevel( log_level );
    logger.clear = cc->logging->clear;
    *cc->logging = logger;
    UA_ClientConfig_setDefault(cc);

    // UA_LogLevel log_level = UA_LOGLEVEL_ERROR;
    // UA_Logger logger = UA_Log_Stdout_withLevel( log_level );
    // logger.clear = cc->logging->clear;
    // *cc->logging = logger;

    /* Set stateCallback */
    cc->stateCallback = stateCallback;
    cc->subscriptionInactivityCallback = subscriptionInactivityCallback;

    UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://m3:48400/UA/ComServerWrapper");
    if(retval != UA_STATUSCODE_GOOD) {
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Not connected.");
    }

    /* Endless loop runAsync */
    while(running) {
        /* if already connected, this will return GOOD and do nothing */
        /* if the connection is closed/errored, the connection will be reset and then reconnected */
        /* Alternatively you can also use UA_Client_getState to get the current state */

        // UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://m3:48400/UA/ComServerWrapper");
        // if(retval != UA_STATUSCODE_GOOD) {
        //     UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        //                  "Not connected. Retrying to connect in 1 second");
        //     /* The connect may timeout after 1 second (see above) or it may fail immediately on network errors */
        //     /* E.g. name resolution errors or unreachable network. Thus there should be a small sleep here */
        //     sleep_ms(1000);
        //     continue;
        // }

        UA_Client_run_iterate(client, 1000);
    }

    /* Clean up - use disconnectAsync and process until fully disconnected */
    UA_Client_disconnectAsync(client);
    
    /* Keep processing until the session is actually closed */
    UA_SecureChannelState channelState;
    UA_SessionState sessionState;
    UA_StatusCode connectStatus;
    
    do {
        UA_Client_run_iterate(client, 100);
        UA_Client_getState(client, &channelState, &sessionState, &connectStatus);
    } while(sessionState != UA_SESSIONSTATE_CLOSED && channelState != UA_SECURECHANNELSTATE_CLOSED);
    
    UA_Client_delete(client);
    return EXIT_SUCCESS;
}
