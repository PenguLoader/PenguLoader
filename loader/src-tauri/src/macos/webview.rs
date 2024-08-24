use std::os::raw::c_void;

use cocoa::{
    appkit::NSWindow,
    base::{id, nil},
};
use objc::declare::ClassDecl;
use objc::runtime::{Object, Sel};
use objc::{class, msg_send};
use objc::{sel, sel_impl};

/// Patch WKWebview to ignore certificate errors.

pub fn setup<R: tauri::Runtime>(window: tauri::Window<R>) {
    unsafe {
        let ns_window = window.ns_window().unwrap() as id;
        let content_view: id = ns_window.contentView(); // WryParentWebView
    
        let subviews: id = msg_send![content_view, subviews];
        let webview: id = msg_send![subviews, objectAtIndex: 0]; // WryWebView
    
        let cls = match ClassDecl::new("WKNavigationDelegate2", class!(NSObject)) {
            Some(mut cls) => {
            //   cls.add_ivar::<*mut c_void>("function");
              cls.add_method(
                sel!(webView:didReceiveAuthenticationChallenge:completionHandler:),
                webview_did_receive_authentication_challenge as extern "C" fn(&Object, Sel, id, id, id),
              );
              cls.register()
            }
            None => class!(WKNavigationDelegate),
          };
  
          let handler: id = msg_send![cls, new];
        //   let nav_handler_ptr = Box::into_raw(Box::new(nav_handler));
        //   (*handler).set_ivar("function", nav_handler_ptr as *mut _ as *mut c_void);

        // Set the delegate on the WebView
        //let _: () = msg_send![webview, setNavigationDelegate: handler];

    }
}

extern "C" fn webview_did_receive_authentication_challenge(
    _self: &Object,
    _cmd: Sel,
    _webview: id,
    challenge: id,
    completion_handler: id,
) {
    unsafe {
        let name = (*_webview).class().name();
        println!("klass: {}", name);
    }
}
