//
//  FileProviderEnumerator.m
//  FileProExt
//
//  Created by chrilarc on 15.07.2026.
//

#import <Foundation/Foundation.h>

#import "FileProviderEnumerator.h"
#import "FileProviderItem.h"

@implementation FileProviderEnumerator

- (void)invalidate {
    // TODO: perform invalidation of server connection if necessary
}

- (void)enumerateItemsForObserver:(nonnull id<NSFileProviderEnumerationObserver>)observer startingAtPage:(nonnull NSFileProviderPage)page {
    /* TODO:
     - inspect the page to determine whether this is an initial or a follow-up request
     
     If this is an enumerator for a directory, the root container or all directories:
     - perform a server request to fetch directory contents
     If this is an enumerator for the active set:
     - perform a server request to update your local database
     - fetch the active set from your local database
     
     - inform the observer about the items returned by the server (possibly multiple times)
     - inform the observer that you are finished with this page
     */
    FileProviderItem *item = [[FileProviderItem alloc] initWithItemIdentifier:@"a file"];
    [observer didEnumerateItems:@[item]];
    [observer finishEnumeratingUpToPage:nil];
}

@end
