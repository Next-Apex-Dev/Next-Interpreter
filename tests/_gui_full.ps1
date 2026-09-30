$exe = "F:\新Next1.1\新Next1.1\next11.exe"
$testFile = [System.IO.Path]::GetTempPath() + "_gui_full.next"
$code = @"
fn main() -> void {
    let w: int = gui_window("Full Test", 400, 300);
    let cv: int = gui_canvas(w, 10, 10, 380, 280);
    gui_draw_line(cv, 0, 0, 100, 100, "red");
    gui_draw_rect(cv, 10, 10, 80, 80, "blue");
    gui_fill_rect(cv, 20, 20, 60, 60, "green");
    gui_draw_circle(cv, 200, 150, 50, "black");
    gui_fill_circle(cv, 200, 150, 30, "yellow");
    gui_draw_text(cv, "Hello", 50, 200, "white");
    gui_clear_canvas(cv);
    print("draw ok");
    let btn: int = gui_button(w, "Click", 150, 250, 100, 30);
    gui_on_click(btn, "onclk");
    gui_set_text(btn, "Clicked");
    print(gui_get_text(btn));
    gui_set_pos(btn, 160, 255);
    gui_set_size(btn, 120, 35);
    gui_set_visible(btn, true);
    gui_set_enabled(btn, false);
    print("widget ok");
    let items: string[] = ["a", "b", "c"];
    let lb: int = gui_listbox(w, items, 300, 10, 80, 100);
    let cb: int = gui_combobox(w, items, 300, 120, 80, 30);
    let ck: int = gui_checkbox(w, "check", 300, 160, 80, 20);
    let rd: int = gui_radio(w, "radio", 300, 185, 80, 20);
    let sb: int = gui_scrollbar(w, 300, 210, 80, 20);
    let tb: int = gui_textbox(w, "text", 10, 260, 100, 25);
    let lbl: int = gui_label(w, "Label", 120, 260, 100, 25);
    print("controls ok");
    gui_show(w);
    print("show ok");
}
fn onclk() -> void { print("clicked"); }
"@
[System.IO.File]::WriteAllText($testFile, $code, [System.Text.UTF8Encoding]::new($false))
$p = [System.Diagnostics.Process]::new()
$p.StartInfo.FileName = $exe
$p.StartInfo.Arguments = '"' + $testFile + '"'
$p.StartInfo.UseShellExecute = $false
$p.StartInfo.RedirectStandardOutput = $true
$p.StartInfo.RedirectStandardError = $true
$p.StartInfo.CreateNoWindow = $true
[void]$p.Start()
[void]$p.WaitForExit(2000)
if (-not $p.HasExited) { $p.Kill(); Write-Host "结果: 窗口弹出成功" }
else { Write-Host "结果: exit=$($p.ExitCode)" }
Write-Host "输出: $($p.StandardOutput.ReadToEnd())"
$serr = $p.StandardError.ReadToEnd()
if ($serr) { Write-Host "错误: $serr" } else { Write-Host "错误: 无" }
Remove-Item $testFile -ErrorAction SilentlyContinue