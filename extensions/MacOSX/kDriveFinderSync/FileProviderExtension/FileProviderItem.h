//
//  FileProviderItem.h
//  FileProviderExtension
//
//  Created by chrilarc on 15.07.2026.
//

#import <FileProvider/FileProvider.h>

@interface FileProviderItem : NSObject<NSFileProviderItem>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithItemIdentifier:(NSFileProviderItemIdentifier)itemIdentifier NS_DESIGNATED_INITIALIZER;

@end
