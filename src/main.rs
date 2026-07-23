mod build;
mod config;
mod diagnostic;
mod event;
mod project;
mod run;
mod tui;

fn main() -> color_eyre::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "warn".into()),
        )
        .init();

    let mut app = tui::app::TuiApplication::new();
    app.run()
}
