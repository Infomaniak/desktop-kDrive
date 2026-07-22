//
//  FileProviderItem.h
//  FileProExt
//
//  Created by chrilarc on 15.07.2026.
//

#import <FileProvider/FileProvider.h>

@interface FileProviderItem : NSObject<NSFileProviderItem>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithTemplate:(NSFileProviderItem)template identifier:(NSFileProviderItemIdentifier)identifier version:(NSString*)version NS_DESIGNATED_INITIALIZER;

@end
