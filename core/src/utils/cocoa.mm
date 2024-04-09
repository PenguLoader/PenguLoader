#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

namespace platform
{
    const char *get_os_version()
    {
        @autoreleasepool
        {
            static char output[24];
            NSOperatingSystemVersion osVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
            snprintf(output, sizeof(output), "%ld.%ld.%ld",
                (long)osVersion.majorVersion,
                (long)osVersion.minorVersion,
                (long)osVersion.patchVersion);
            return output;
        }
    }

    const char *get_os_build()
    {
        @autoreleasepool
        {
            static char output[32];
            NSDictionary *systemVersionPlist = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
            NSString *buildNumber = systemVersionPlist[@"ProductBuildVersion"];
            snprintf(output, sizeof(output), "%s", [buildNumber UTF8String]);
            return output;
        }
    }
}

namespace shell
{
    void open_url(const char *url)
    {
        @autoreleasepool
        {
            NSString *str = [NSString stringWithUTF8String:url];
            NSURL *nsurl = [NSURL URLWithString:str];
            [[NSWorkspace sharedWorkspace] openURL:nsurl];
        }
    }

    void open_folder_utf8(const char *path)
    {
        @autoreleasepool
        {
            NSString *str = [NSString stringWithUTF8String:path];
            NSURL *url = [NSURL fileURLWithPath:str isDirectory:YES];
            [[NSWorkspace sharedWorkspace] openURL:url];
        }
    }
}

namespace dialog
{
    void alert(const char *message, const char *caption)
    {
        @autoreleasepool
        {
            NSAlert *alert = [[NSAlert alloc] init];
            NSString *messageString = [NSString stringWithUTF8String:message];
            NSString *captionString = [NSString stringWithUTF8String:caption];

            [alert setMessageText:captionString];
            [alert setInformativeText:messageString];
            [alert addButtonWithTitle:@"OK"];

            [alert runModal];
        }
    }

    bool confirm(const char *message, const char *caption)
    {
        @autoreleasepool
        {
            NSAlert *alert = [[NSAlert alloc] init];
            NSString *messageString = [NSString stringWithUTF8String:message];
            NSString *captionString = [NSString stringWithUTF8String:caption];
            
            [alert setMessageText:captionString];
            [alert setInformativeText:messageString];
            [alert setAlertStyle:NSAlertStyleCritical];
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];

            NSInteger response = [alert runModal];
            return response == NSAlertFirstButtonReturn;
        }
    }
}

namespace window
{
    void get_rect(void *nsview, int *x, int *y, int *w, int *h)
    {
        NSView *view = (__bridge NSView *)nsview;
        NSRect frameInWindow = [view convertRect:[view bounds] toView:nil];
        NSRect frameOnScreen = [view.window convertRectToScreen:frameInWindow];

        if (x) *x = frameOnScreen.origin.x;
        if (y) *y = frameOnScreen.origin.y;
        if (w) *w = frameOnScreen.size.width;
        if (h) *h = frameOnScreen.size.height;
    }

    float get_scaling(void *nsview)
    {
        return 1.0f;
    }

    void make_foreground(void *nsview)
    {
        NSView *view = (__bridge NSView *)nsview;
        [view.window orderFrontRegardless];
    }

    void clear_vibrancy(void *nsview)
    {
        @autoreleasepool
        {
            NSView *view = (__bridge NSView *)nsview;

            if (view.subviews.count > 1)
                [[view.subviews firstObject] removeFromSuperview];
        }
    }

    void apply_vibrancy(void *nsview, int _material, bool follow_active)
    {
        clear_vibrancy(nsview);

        @autoreleasepool
        {
            NSView *view = (__bridge NSView *)nsview;
            NSRect rect = view.bounds;
            
            if (view.subviews.count > 1)
                [[view.subviews firstObject] removeFromSuperview];

            NSVisualEffectMaterial material = (NSVisualEffectMaterial)_material;
            NSVisualEffectState state = follow_active ? NSVisualEffectStateFollowsWindowActiveState : NSVisualEffectStateActive;
            NSVisualEffectView *blurredView = [[[NSVisualEffectView alloc] initWithFrame:rect] autorelease];
            
            [blurredView setMaterial:material];
            [blurredView setState:state];
            [blurredView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
            
            // if (blurredView.layer)
            //     blurredView.layer.cornerRadius = cornerRadius;
            
            blurredView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            [view addSubview:blurredView positioned:NSWindowBelow relativeTo:0];
        }
    }

    void enable_shadow(void *nsview)
    {
        NSView *view = (__bridge NSView *)nsview;
        [view.window invalidateShadow];
    }

    bool is_dark_theme()
    {
        @autoreleasepool
        {
            if ([[NSApplication sharedApplication] respondsToSelector:@selector(effectiveAppearance)])
            {
                NSAppearance *appearance = [[NSApplication sharedApplication] effectiveAppearance];

                if ([appearance.name rangeOfString:@"dark" options:NSCaseInsensitiveSearch].location != NSNotFound)
                    return true;
            }

            return false;
        }
    }

    void set_theme(void *nsview, bool dark)
    {
        @autoreleasepool
        {
            NSView *view = (__bridge NSView *)nsview;
            NSAppearanceName theme = dark ? NSAppearanceNameVibrantDark : NSAppearanceNameVibrantLight;
            NSAppearance *appearance = [NSAppearance appearanceNamed:theme];

            [view.window setAppearance:appearance];
            [view.window invalidateShadow];
        }
    }
}