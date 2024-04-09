#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

namespace shell
{
    void open_url(const char *url)
    {
        NSString *str = [NSString stringWithUTF8String:url];
        NSURL *nsurl = [NSURL URLWithString:str];
        [[NSWorkspace sharedWorkspace] openURL:nsurl];
    }

    void open_folder_utf8(const char *path)
    {
        NSString *str = [NSString stringWithUTF8String:path];
        NSURL *url = [NSURL fileURLWithPath:str isDirectory:YES];
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

namespace dialog
{
    void alert(const char *message, const char *caption)
    {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        NSString *messageString = [NSString stringWithUTF8String:message];
        NSString *captionString = [NSString stringWithUTF8String:caption];

        [alert setMessageText:captionString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"OK"];

        [alert runModal];
    }

    bool confirm(const char *message, const char *caption)
    {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
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

    void apply_vibrancy(void *nsview, int material, int state, double cornerRadius)
    {
        NSView *view = (__bridge NSView *)nsview;
        NSRect rect = view.bounds;
        
        NSVisualEffectMaterial m = (NSVisualEffectMaterial)material;
        NSVisualEffectView *blurredView = [[[NSVisualEffectView alloc] initWithFrame:rect] autorelease];
        
        [blurredView setMaterial:m];
        [blurredView setState:(NSVisualEffectState)state];
        [blurredView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
        
        if (blurredView.layer)
            blurredView.layer.cornerRadius = cornerRadius;
        
        blurredView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        
        [view addSubview:blurredView positioned:NSWindowBelow relativeTo:0];
    }

    void enable_shadow(void *nsview)
    {
        NSView *view = (__bridge NSView *)nsview;
        [view.window invalidateShadow];
    }

    bool is_dark_theme()
    {
        if ([[NSApplication sharedApplication] respondsToSelector:@selector(effectiveAppearance)])
        {
            NSAppearance *appearance = [[NSApplication sharedApplication] effectiveAppearance];

            if ([appearance.name rangeOfString:@"dark" options:NSCaseInsensitiveSearch].location != NSNotFound)
                return true;
        }

        return false;
    }

    void set_theme(void *nsview, bool dark)
    {
        NSView *view = (__bridge NSView *)nsview;
        NSAppearanceName theme = dark ? NSAppearanceNameVibrantDark : NSAppearanceNameVibrantLight;
        NSAppearance *appearance = [NSAppearance appearanceNamed:theme];

        [view.window setAppearance:appearance];
        [view.window invalidateShadow];
    }
}