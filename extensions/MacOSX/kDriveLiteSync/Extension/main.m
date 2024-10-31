/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import "../../../../src/libcommonserver/io/fileAttributes.h"
#include "xpcService.h"

#import <Foundation/Foundation.h>
#import <EndpointSecurity/EndpointSecurity.h>
#import <bsm/libbsm.h>

#include <dispatch/queue.h>
#include <sys/xattr.h>

es_client_t *g_client = NULL;
XPCService *g_xpcService = NULL;
static dispatch_queue_t g_event_queue = NULL;

static void initDispatchQueue(void)
{
    dispatch_queue_attr_t queue_attrs = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INITIATED, 0);
    g_event_queue = dispatch_queue_create("event_queue", queue_attrs);
}

// Clean-up before exiting
void sig_handler(int sig)
{
    NSLog(@"[KD] Tidying Up");
    
    if (g_client) {
        if (@available(macOS 10.15, *)) {
            es_unsubscribe_all(g_client);
            es_delete_client(g_client);
        } else {
            // Fallback on earlier versions
        }
    }
    
    NSLog(@"[KD] Exiting");
    exit(EXIT_SUCCESS);
}

NSString *fileDefaultOpeningAppId(NSString *path)
{
    CFStringRef fileUTI = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (__bridge CFStringRef)[path pathExtension], NULL);
    if (fileUTI) {
        CFURLRef appURLRef = LSCopyDefaultApplicationURLForContentType(fileUTI, kLSRolesAll, nil);
        if (appURLRef) {
            return [[NSBundle bundleWithURL:(__bridge NSURL *)appURLRef] bundleIdentifier];
        }
    }
    
    return nil;
}

BOOL fileHasCustomIcon(NSString *path)
{
    NSError *error;
    NSArray *keys = [NSArray arrayWithObject:NSURLCustomIconKey];
    NSDictionary *values = [[NSURL URLWithString:path] resourceValuesForKeys:keys error:&error];
    if (values == nil) {
        NSLog(@"[KD] ERROR: resourceValuesForKeys() failed for file/directory %@: %@", path, error.description);
        return FALSE;
    }
    
    if ([[values objectForKey:NSURLCustomIconKey] boolValue]) {
        return TRUE;
    }

    return FALSE;
}

// Filter auth open events
static BOOL processAuthOpen(const es_message_t *msg, BOOL *thumbnail)
{
    if (msg->process->is_es_client) {
        return FALSE;
    }
        
    // Check open flag
    if ((msg->event.open.fflag & O_DIRECTORY)
        || (msg->event.open.fflag & O_SYMLINK)
        || (msg->event.open.fflag & O_NONBLOCK)) {
        return FALSE;
    }
    
    // Check that the file is being monitored
    NSString *filePath = [NSString stringWithUTF8String:msg->event.open.file->path.data];
    if (!(g_xpcService && [g_xpcService isFileMonitored:filePath])) {
        return FALSE;
    }
        
    /*NSLog(@"[KD] Open file %s with flags %d asked by %s",
          msg->event.open.file->path.data, msg->event.open.fflag,
          msg->process->signing_id.data);*/
    
    // Check file status
    long bufferLength = getxattr([filePath UTF8String], [@EXT_ATTR_STATUS UTF8String], NULL, 0, 0, 0);
    if (bufferLength >= 0) {
        char status[bufferLength];
        if (getxattr([filePath UTF8String], [@EXT_ATTR_STATUS UTF8String], status, bufferLength, 0, 0)
            != bufferLength) {
            NSLog(@"[KD] ERROR: fgetxattr() failed for file %@: %d", filePath, errno);
            return FALSE;
        }

        if (status[0] != EXT_ATTR_STATUS_ONLINE[0]) {
            // The file is not dehydrated.
            return FALSE;
        }
    }
    else {
        return FALSE;
    }
    
    // Check opening process
    if (msg->process->signing_id.data) {
        NSString *appId = [NSString stringWithUTF8String:msg->process->signing_id.data];
        
        // Do not fetch file if the process is in the forbidden list
        if (g_xpcService && [g_xpcService isAppInOpenBlackList:appId filePath:filePath]) {
            return FALSE;
        }

        // Fetch file if the process is in the allowed list
        if (g_xpcService && [g_xpcService isAppInOpenWhiteList:appId]) {
            *thumbnail = FALSE;
            return TRUE;
        }

        // Fetch file if the process is the default opening app
        NSString *defaultAppId = fileDefaultOpeningAppId(filePath);
        if (defaultAppId && [appId isEqualToString:defaultAppId]) {
            *thumbnail = FALSE;
            return TRUE;
        }
        
        // Fetch thumbnail if the process is in the thumbnail allowed list and the file doesn't already have a custom icon
        if (g_xpcService && [g_xpcService isAppInOpenWhiteListThumbnail:appId]) {
            if (fileHasCustomIcon(filePath)) {
                return FALSE;
            }
            else {
                *thumbnail = TRUE;
                return TRUE;
            }
        }

        // Fetch file only if `filePath` doesn't indicate a directory.
        if (g_xpcService) {
            [g_xpcService addAppToFetchList:appId];

            NSError *error = nil;
            if ([g_xpcService isDirectory:filePath error:&error]) {
                NSLog(@"[KD] The app '%@' asked to hydrate the entire folder '%@': authorization denied.", appId, filePath);
                return FALSE;
            } else if (error) {
                NSLog(@"[KD] ERROR: xpcService isDirectory() failed for path='%@', error='%@'", filePath, error);
                return FALSE;
            }
        }

        *thumbnail = FALSE;
        return TRUE;
    }
    
    return FALSE;
}

// Filter auth rename events
static BOOL processAuthRename(const es_message_t *msg)
{
    if (msg->process->is_es_client) {
        return FALSE;
    }
        
    // Check that the file is being monitored
    NSString *filePath = [NSString stringWithUTF8String:msg->event.rename.source->path.data];
    if (!(g_xpcService && [g_xpcService isFileMonitored:filePath])) {
        return FALSE;
    }
    
    // Check file status
    long bufferLength = getxattr([filePath UTF8String], [@EXT_ATTR_STATUS UTF8String], NULL, 0, 0, 0);
    char status[bufferLength];
    if (bufferLength >= 0) {
        if (getxattr([filePath UTF8String], [@EXT_ATTR_STATUS UTF8String], status, bufferLength, 0, 0)
            != bufferLength) {
            NSLog(@"[KD] ERROR: fgetxattr() failed for file %@: %d", filePath, errno);
            return FALSE;
        }

        if (status[0] != EXT_ATTR_STATUS_ONLINE[0]) {
            return FALSE;
        }
    }
    else {
        return FALSE;
    }
    
    // Check that the destination is the Trash
    NSString *destinationPath = [NSString stringWithUTF8String:msg->event.rename.destination.new_path.dir->path.data];
    if ([destinationPath hasSuffix:@".Trash"]){
        return TRUE;
    }
    
    return FALSE;
}

static void handleAuthOpenWorker(es_client_t *client, es_message_t *msg)
{
    BOOL thumbnail = FALSE;
    if (processAuthOpen(msg, &thumbnail)) {
        //uint64_t timeoutInterval = (uint64_t)((msg->deadline - msg->mach_time) / 1.0e9);
        //NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:timeoutInterval];

        NSString *filePath = [NSString stringWithUTF8String:msg->event.open.file->path.data];
        pid_t pid = audit_token_to_pid(msg->process->audit_token);

        if (thumbnail) {
            NSLog(@"[KD] Fetch thumbnail %s with flags %d asked by %s",
                  msg->event.open.file->path.data, msg->event.open.fflag,
                  msg->process->signing_id.data);
            
            if (![g_xpcService fetchThumbnail:filePath pid:pid]) {
                NSLog(@"[KD] ERROR: fetchThumbnail() failed");
            }
        }
        else {
            NSLog(@"[KD] Fetch file %s with flags %d asked by %s",
                  msg->event.open.file->path.data, msg->event.open.fflag,
                  msg->process->signing_id.data);
            
            if (![g_xpcService fetchFile:filePath pid:pid]) {
                NSLog(@"[KD] ERROR: fetchFile() failed");
            }
        }
    }

    // Allow open
    uint32_t authorized_flags = 0xffffffff;
    es_respond_result_t res = es_respond_flags_result(client, msg, authorized_flags, true);
    if (res != ES_RESPOND_RESULT_SUCCESS) {
        NSLog(@"[KD] ERROR: es_respond_flags_result() failed : %d", res);
    }
}

static void handleAuthRenameWorker(es_client_t *client, es_message_t *msg)
{
    if (processAuthRename(msg)) {
        NSLog(@"[KD] Move file %s to trash asked by %s",
              msg->event.rename.source->path.data,
              msg->process->signing_id.data);
    }

    es_respond_auth_result(client, msg, ES_AUTH_RESULT_ALLOW, true);
}

static void handleAuthOpen(es_client_t *client, const es_message_t *msg)
{
    es_message_t *copied_msg = es_copy_message(msg);

    dispatch_async(g_event_queue, ^{
        handleAuthOpenWorker(client, copied_msg);
        es_free_message(copied_msg);
    });
}

static void handleAuthRename(es_client_t *client, const es_message_t *msg)
{
    es_message_t *copied_msg = es_copy_message(msg);

    dispatch_async(g_event_queue, ^{
        handleAuthRenameWorker(client, copied_msg);
        es_free_message(copied_msg);
    });
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &sig_handler);
    signal(SIGSEGV, &sig_handler);
    
    initDispatchQueue();
    
    // Initialize XPC cLient
    NSLog(@"[KD] Initialize XPC client");
    g_xpcService = [[XPCService alloc] init];

    // Create Endpoint Security client
    NSLog(@"[KD] Create Endpoint Security client");
    es_new_client_result_t res = es_new_client(&g_client, ^(es_client_t *client, const es_message_t *msg)
    {
        switch (msg->event_type) {
            case ES_EVENT_TYPE_AUTH_OPEN:
                handleAuthOpen(client, msg);
                break;
            case ES_EVENT_TYPE_AUTH_RENAME:
                handleAuthRename(client, msg);
                break;
            default:
                if (msg->action_type == ES_ACTION_TYPE_AUTH) {
                    es_respond_result_t res = es_respond_auth_result(client, msg, ES_AUTH_RESULT_ALLOW, true);
                    if(res != ES_RESPOND_RESULT_SUCCESS) {
                        NSLog(@"[KD] ERROR: es_respond_auth_result() failed : %d", res);
                    }
                }
                break;
        }
    });
    
    if (res != ES_NEW_CLIENT_RESULT_SUCCESS) {
        if (res == ES_NEW_CLIENT_RESULT_ERR_NOT_ENTITLED) {
            NSLog(@"[KD] ERROR: Application requires entitlement.");
        } else if (res == ES_NEW_CLIENT_RESULT_ERR_NOT_PERMITTED) {
            NSLog(@"[KD] ERROR: Application needs to run as root (and SIP disabled).");
        } else {
            NSLog(@"[KD] ERROR: es_new_client() failed : %d", res);
        }
        return 1;
    }
    
    // Subscribe to the events we're interested in
    NSLog(@"[KD] Subscribe to events");
    es_event_type_t events[] = {
        ES_EVENT_TYPE_AUTH_OPEN,
        //ES_EVENT_TYPE_AUTH_RENAME
    };
    
    es_return_t ret = es_subscribe(g_client, events, (sizeof(events) / sizeof((events)[0])));
    if (ret == ES_RETURN_ERROR) {
        NSLog(@"[KD] ERROR: es_subscribe() failed.");
        return 1;
    }
    
    dispatch_main();
    
    return 0;
}
