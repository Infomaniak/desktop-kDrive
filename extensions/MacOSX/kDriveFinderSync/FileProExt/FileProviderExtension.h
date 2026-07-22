//
//  FileProviderExtension.h
//  FileProExt
//
//  Created by chrilarc on 15.07.2026.
//

#import <FileProvider/FileProvider.h>

#import "xpcClientProxy.h"

@interface FileProviderExtension : NSObject<NSFileProviderReplicatedExtension, XPCClientProxyDelegate> {
    XPCClientProxy *_xpcClientProxy;
}

@end
