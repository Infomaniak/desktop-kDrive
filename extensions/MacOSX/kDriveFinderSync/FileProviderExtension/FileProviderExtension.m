//
//  FileProviderExtension.m
//  FileProviderExtension
//
//  Created by chrilarc on 15.07.2026.
//

#import <Foundation/Foundation.h>

#import "FileProviderExtension.h"
#import "FileProviderEnumerator.h"
#import "FileProviderItem.h"

@implementation FileProviderExtension

- (nonnull instancetype)initWithDomain:(nonnull NSFileProviderDomain *)domain {
    // TODO: The containing application must create a domain using `+[NSFileProviderManager addDomain:completionHandler:]`. The system will then launch the application extension process, call `-[FileProviderExtension initWithDomain:]` to instantiate the extension for that domain, and call methods on the instance.
    self = [super init];
    return self;
}

- (void)invalidate {
    // TODO: cleanup any resources
}

- (nullable id<NSFileProviderEnumerator>)enumeratorForContainerItemIdentifier:(nonnull NSFileProviderItemIdentifier)containerItemIdentifier request:(nonnull NSFileProviderRequest *)request error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return [[FileProviderEnumerator alloc] init];
}

- (nonnull NSProgress *)createItemBasedOnTemplate:(nonnull NSFileProviderItem)itemTemplate fields:(NSFileProviderItemFields)fields contents:(nullable NSURL *)url options:(NSFileProviderCreateItemOptions)options request:(nonnull NSFileProviderRequest *)request completionHandler:(nonnull void (^)(NSFileProviderItem _Nullable, NSFileProviderItemFields, BOOL, NSError * _Nullable))completionHandler {
    // TODO: a new item was created on disk, process the item's creation

    NSFileProviderItemFields remainingFields = 0;
    completionHandler(itemTemplate, remainingFields, false, nil);
    return [[NSProgress alloc] init];
}

- (nonnull NSProgress *)deleteItemWithIdentifier:(nonnull NSFileProviderItemIdentifier)identifier baseVersion:(nonnull NSFileProviderItemVersion *)version options:(NSFileProviderDeleteItemOptions)options request:(nonnull NSFileProviderRequest *)request completionHandler:(nonnull void (^)(NSError * _Nullable))completionHandler {
    // TODO: an item was deleted on disk, process the item's deletion

    NSError *error = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    completionHandler(error);
    return [[NSProgress alloc] init];
}

- (nonnull NSProgress *)fetchContentsForItemWithIdentifier:(nonnull NSFileProviderItemIdentifier)itemIdentifier version:(nullable NSFileProviderItemVersion *)requestedVersion request:(nonnull NSFileProviderRequest *)request completionHandler:(nonnull void (^)(NSURL * _Nullable, NSFileProviderItem _Nullable, NSError * _Nullable))completionHandler {
    // TODO: implement fetching of the contents for the itemIdentifier at the specified version

    NSError *error = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    completionHandler(nil, nil, error);
    return [[NSProgress alloc] init];
}

- (nonnull NSProgress *)itemForIdentifier:(nonnull NSFileProviderItemIdentifier)identifier request:(nonnull NSFileProviderRequest *)request completionHandler:(nonnull void (^)(NSFileProviderItem _Nullable, NSError * _Nullable))completionHandler {
    // resolve the given identifier to a record in the model

    // TODO: implement the actual lookup

    completionHandler([[FileProviderItem alloc] initWithItemIdentifier:identifier], nil);
    return [[NSProgress alloc] init];
}

- (nonnull NSProgress *)modifyItem:(nonnull NSFileProviderItem)item baseVersion:(nonnull NSFileProviderItemVersion *)version changedFields:(NSFileProviderItemFields)changedFields contents:(nullable NSURL *)newContents options:(NSFileProviderModifyItemOptions)options request:(nonnull NSFileProviderRequest *)request completionHandler:(nonnull void (^)(NSFileProviderItem _Nullable, NSFileProviderItemFields, BOOL, NSError * _Nullable))completionHandler {
    // TODO: an item was modified on disk, process the item's modification
    
    NSFileProviderItemFields remainingFields = 0;
    NSError *error = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    completionHandler(nil, remainingFields, false, error);
    return [[NSProgress alloc] init];
}

@end
