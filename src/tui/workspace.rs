use ratatui::layout::{Alignment, Rect};
use ratatui::style::{Color, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, List, ListItem, Paragraph, Wrap};
use ratatui::Frame;

use crate::build::BuildStage;
use crate::config::ThemeColors;

fn hex_color(hex: &str) -> Color {
    let hex = hex.trim_start_matches('#');
    if hex.len() == 6 {
        if let (Ok(r), Ok(g), Ok(b)) = (
            u8::from_str_radix(&hex[0..2], 16),
            u8::from_str_radix(&hex[2..4], 16),
            u8::from_str_radix(&hex[4..6], 16),
        ) {
            return Color::Rgb(r, g, b);
        }
    }
    Color::White
}

pub fn render_file_panel(frame: &mut Frame, area: Rect, focused: bool, colors: &ThemeColors) {
    let border_color = if focused {
        hex_color(&colors.panel_border_focused)
    } else {
        hex_color(&colors.panel_border_unfocused)
    };
    let fg_color = if focused {
        hex_color(&colors.foreground)
    } else {
        hex_color(&colors.list_inactive_foreground)
    };
    let block = Block::bordered()
        .title(" Files ")
        .border_style(Style::default().fg(border_color));
    let items: Vec<ListItem> = vec![
        "src/main.cpp",
        "src/engine.cpp",
        "include/engine.hpp",
        "CMakeLists.txt",
    ]
    .iter()
    .map(|s| ListItem::new(Line::from(Span::styled(*s, Style::default().fg(fg_color)))))
    .collect();
    let list = List::new(items).block(block);
    frame.render_widget(list, area);
}

pub fn render_target_panel(frame: &mut Frame, area: Rect, focused: bool, colors: &ThemeColors) {
    let border_color = if focused {
        hex_color(&colors.panel_border_focused)
    } else {
        hex_color(&colors.panel_border_unfocused)
    };
    let fg_color = if focused {
        hex_color(&colors.foreground)
    } else {
        hex_color(&colors.list_inactive_foreground)
    };
    let block = Block::bordered()
        .title(" Targets ")
        .border_style(Style::default().fg(border_color));
    let items: Vec<ListItem> = vec!["my_app (executable)", "my_lib (static)"]
        .iter()
        .map(|s| ListItem::new(Line::from(Span::styled(*s, Style::default().fg(fg_color)))))
        .collect();
    let list = List::new(items).block(block);
    frame.render_widget(list, area);
}

pub fn render_output_panel(
    frame: &mut Frame,
    area: Rect,
    focused: bool,
    colors: &ThemeColors,
    lines: &[String],
) {
    let border_color = if focused {
        hex_color(&colors.panel_border_focused)
    } else {
        hex_color(&colors.panel_border_unfocused)
    };
    let fg_color = if focused {
        hex_color(&colors.foreground)
    } else {
        hex_color(&colors.list_inactive_foreground)
    };
    let block = Block::bordered()
        .title(" Output ")
        .border_style(Style::default().fg(border_color));
    let content = if lines.is_empty() {
        "No output yet.".to_string()
    } else {
        lines.join("\n")
    };
    let para = Paragraph::new(content)
        .style(Style::default().fg(fg_color))
        .block(block)
        .wrap(Wrap { trim: false });
    frame.render_widget(para, area);
}

pub fn render_build_overlay(
    frame: &mut Frame,
    area: Rect,
    log_lines: &[String],
    stage: BuildStage,
    is_building: bool,
    colors: &ThemeColors,
) {
    let overlay_area = centered_rect(60, 50, area);
    let fg = hex_color(&colors.foreground);
    let bg = hex_color(&colors.background);

    let status = if is_building {
        format!("Building... [{}]", stage.as_str())
    } else {
        format!("Build: {}", stage.as_str())
    };

    let mut items = vec![
        Line::from(Span::styled(" Build ", Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(40), Style::default().fg(fg))),
        Line::from(Span::styled(&status, Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(40), Style::default().fg(fg))),
    ];
    for line in log_lines.iter().rev().take(20) {
        items.push(Line::from(Span::styled(line, Style::default().fg(fg))));
    }
    items.push(Line::from(Span::styled(
        " 'b': build   'c': cancel   'esc': close ",
        Style::default().fg(hex_color(&colors.list_inactive_foreground)),
    )));

    let para = Paragraph::new(Vec::from(items))
        .style(Style::default().bg(bg))
        .block(Block::bordered().border_style(Style::default().fg(fg)));
    frame.render_widget(para, overlay_area);
}

pub fn render_run_overlay(
    frame: &mut Frame,
    area: Rect,
    log_lines: &[String],
    is_running: bool,
    colors: &ThemeColors,
) {
    let overlay_area = centered_rect(60, 50, area);
    let fg = hex_color(&colors.foreground);
    let bg = hex_color(&colors.background);

    let status = if is_running { "Running..." } else { "Ready" };

    let mut items = vec![
        Line::from(Span::styled(" Run ", Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(40), Style::default().fg(fg))),
        Line::from(Span::styled(status, Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(40), Style::default().fg(fg))),
    ];
    for line in log_lines.iter().rev().take(20) {
        items.push(Line::from(Span::styled(line, Style::default().fg(fg))));
    }
    items.push(Line::from(Span::styled(
        " 'r': run   'k': kill   'esc': close ",
        Style::default().fg(hex_color(&colors.list_inactive_foreground)),
    )));

    let para = Paragraph::new(Vec::from(items))
        .style(Style::default().bg(bg))
        .block(Block::bordered().border_style(Style::default().fg(fg)));
    frame.render_widget(para, overlay_area);
}

pub fn render_help_overlay(frame: &mut Frame, area: Rect, colors: &ThemeColors) {
    let overlay_area = centered_rect(50, 40, area);
    let fg = hex_color(&colors.foreground);
    let bg = hex_color(&colors.background);
    let dim = hex_color(&colors.list_inactive_foreground);

    let items = vec![
        Line::from(Span::styled(" Help ", Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(30), Style::default().fg(fg))),
        Line::from(Span::styled(" q        Quit / Go back", Style::default().fg(fg))),
        Line::from(Span::styled(" Tab      Next panel", Style::default().fg(fg))),
        Line::from(Span::styled(" b        Build overlay", Style::default().fg(fg))),
        Line::from(Span::styled(" c        Cancel build", Style::default().fg(fg))),
        Line::from(Span::styled(" r        Run overlay", Style::default().fg(fg))),
        Line::from(Span::styled(" k        Kill process", Style::default().fg(fg))),
        Line::from(Span::styled(" h        Toggle help", Style::default().fg(fg))),
        Line::from(Span::styled(" Esc      Close overlay", Style::default().fg(fg))),
        Line::from(Span::styled("─".repeat(30), Style::default().fg(dim))),
        Line::from(Span::styled(" Press h or Esc to close", Style::default().fg(dim))),
    ];

    let para = Paragraph::new(Vec::from(items))
        .style(Style::default().bg(bg))
        .block(Block::bordered().border_style(Style::default().fg(fg)));
    frame.render_widget(para, overlay_area);
}

fn centered_rect(percent_x: u16, percent_y: u16, area: Rect) -> Rect {
    let popup_layout = ratatui::layout::Layout::default()
        .direction(ratatui::layout::Direction::Vertical)
        .constraints([
            ratatui::layout::Constraint::Percentage((100 - percent_y) / 2),
            ratatui::layout::Constraint::Percentage(percent_y),
            ratatui::layout::Constraint::Percentage((100 - percent_y) / 2),
        ])
        .split(area);

    ratatui::layout::Layout::default()
        .direction(ratatui::layout::Direction::Horizontal)
        .constraints([
            ratatui::layout::Constraint::Percentage((100 - percent_x) / 2),
            ratatui::layout::Constraint::Percentage(percent_x),
            ratatui::layout::Constraint::Percentage((100 - percent_x) / 2),
        ])
        .split(popup_layout[1])[1]
}
