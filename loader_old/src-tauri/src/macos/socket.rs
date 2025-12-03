use std::net::SocketAddr;
use std::net::TcpStream;
use std::path::PathBuf;
use std::time::Duration;

use native_tls::TlsConnector;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use tungstenite::client_tls_with_config;
use tungstenite::{client::IntoClientRequest, http::HeaderValue, Connector, Message};

use super::utils;
use crate::dprintln;

///
/// The method is quite simple.
/// 1. Get RiotClient command line.
/// 2. Subscribe to the RCS WebSocket (wamp).
/// 3. Listen to the League Client, track it starts and gets closing.
///
/// Install the core module on Client opening and uninstall it after closed.
/// Put them together in the daemon thread and follow active state from the app.
/// 
/// todo: track more events like client update
///

const TIMEOUT: Duration = Duration::from_millis(200);

#[derive(Serialize, Deserialize, Debug)]
struct Payload(i32, String, EventData);

#[derive(Serialize, Deserialize, Debug)]
#[allow(non_snake_case)]
struct EventData {
    pub data: Value,
    pub eventType: String,
    pub uri: String,
}

fn get_lol_dir(conf: &Value) -> Option<PathBuf> {
    if let Some(configuration) = conf.as_object() {
        if let Some(executable) = configuration.get("executable") {
            let path = executable
                .as_str()
                .unwrap()
                .replace("/LeagueClient.app/Contents/MacOS/LeagueClient", "")
                .replace("\"", "");
            return Some(PathBuf::from(path));
        }
    }
    None
}

/// Handle websocket json message.
fn process_message(json: &str) {
    let payload: Payload = match serde_json::from_str(json) {
        Ok(v) => v,
        _ => {
            return;
        }
    };

    // the OnJsonApiEvent doesn't like trailing slash or underscore
    // should re-check the endpoint to make sure 'sessions/' has a slash
    // the last part is launch session ID but unused
    if payload.0 == 8 && payload.2.uri.starts_with("/product-session/v1/sessions/") {
        if let Some(data) = payload.2.data.as_object() {
            if let (Some(product_id), Some(conf)) =
                (data.get("productId"), data.get("launchConfiguration"))
            {
                if product_id == "league_of_legends" {
                    if let Some(lol_dir) = get_lol_dir(conf) {
                        match payload.2.eventType.as_ref() {
                            "Create" => on_open_client(&lol_dir),
                            "Delete" => on_close_client(&lol_dir),
                            _ => (),
                        }
                    }
                }
            }
        }
    }
}

/// Connect to RSC websocket and process events synchronously.
fn connect(addr: &SocketAddr, auth: &String) -> Result<(), std::io::Error> {
    // build request
    let url = format!("wss://{}", addr);
    let mut request = url.clone().into_client_request().unwrap();
    // add auth header
    request.headers_mut().insert(
        "Authorization",
        HeaderValue::from_str(&format!("Basic {}", auth)).unwrap(),
    );

    // ignore certificate errors
    let tls = TlsConnector::from(
        native_tls::TlsConnector::builder()
            .danger_accept_invalid_certs(true)
            .build()
            .unwrap(),
    );

    // websocket stream
    let tcp_stream = TcpStream::connect_timeout(addr, TIMEOUT)?;
    let (mut stream, _) = client_tls_with_config(
        request.clone(),
        tcp_stream,
        None,
        Some(Connector::NativeTls(tls.clone())),
    )
    .expect("The TLS handshake should never fail");

    dprintln!("socket connected to {}", url);

    _ = stream.send(Message::text(
        // a perfect endpoint
        "[5, \"OnJsonApiEvent_product-session_v1_sessions\"]",
    ));

    loop {
        match stream.read() {
            Ok(Message::Text(json)) => {
                if json.is_empty() {
                    // the first empty string means success
                    dprintln!("socket event subscribed");
                } else {
                    process_message(json.as_str());
                }
            }
            Ok(Message::Close(_close)) => {
                dprintln!("socket closed: {:?}", _close);
                break;
            }
            Err(_e) => {
                dprintln!("socket error: {:?}", _e);
                break;
            }
            _ => {}
        }
    }

    Ok(())
}

pub fn run_daemon() {
    dprintln!("run socket daemon");
    std::thread::spawn(|| loop {
        if let Some((addr, auth)) = utils::get_riotclient_info() {
            if let Err(_e) = connect(&addr, &auth) {
                dprintln!("socket failed to connect: {:?}", _e);
            }
        }
        std::thread::sleep(TIMEOUT);
    });
}

fn on_open_client(lol_dir: &PathBuf) {
    dprintln!("on_open_client@lol_dir: {}", lol_dir.display());
    if super::cmd_is_active() {
        super::core::install_module(lol_dir);
    }
}

fn on_close_client(lol_dir: &PathBuf) {
    dprintln!("on_close_client@lol_dir: {}", lol_dir.display());
    super::core::uninstall_module(lol_dir);
}
