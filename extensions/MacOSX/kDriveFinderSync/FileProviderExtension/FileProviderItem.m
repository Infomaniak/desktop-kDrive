//
//  FileProviderItem.m
//  FileProviderExtension
//
//  Created by chrilarc on 15.07.2026.
//

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "FileProviderItem.h"

@implementation FileProviderItem

@synthesize filename = _filename;
@synthesize itemIdentifier = _itemIdentifier;
@synthesize parentItemIdentifier = _parentItemIdentifier;
@synthesize capabilities = _capabilities;
@synthesize itemVersion = _itemVersion;
@synthesize contentType = _contentType;

- (instancetype)initWithItemIdentifier:(NSFileProviderItemIdentifier)identifier {
    self = [super init];
    if (self != nil) {
        _filename = [identifier copy];
        _itemIdentifier = [identifier copy];
        _parentItemIdentifier = NSFileProviderRootContainerItemIdentifier;
        _capabilities = NSFileProviderItemCapabilitiesAllowsReading | NSFileProviderItemCapabilitiesAllowsWriting | NSFileProviderItemCapabilitiesAllowsRenaming | NSFileProviderItemCapabilitiesAllowsReparenting | NSFileProviderItemCapabilitiesAllowsDeleting | NSFileProviderItemCapabilitiesAllowsTrashing;
        _itemVersion = [[NSFileProviderItemVersion alloc] initWithContentVersion:[@"a content version" dataUsingEncoding:NSUTF8StringEncoding] metadataVersion:[@"a metadata version" dataUsingEncoding:NSUTF8StringEncoding]];
        _contentType = ([identifier isEqualToString:NSFileProviderRootContainerItemIdentifier]) ? UTTypeFolder : UTTypePlainText;
    }

    return self;
}

@end
