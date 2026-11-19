using Windows.Media.Control;

// dotnet run -- probe [OBS WebSocket のパスワード]
if (args.Length > 0 && args[0] == "probe")
{
    await Probe.Run(args.Length > 1 ? args[1] : "");
    return;
}

// dotnet run -- watch [ソース名] [窓の名前に含む語] [パスワード]
if (args.Length > 0 && args[0] == "watch")
{
    await Watch.Run(args.Length > 1 ? args[1] : "ウィンドウキャプチャ 2",
                    args.Length > 2 ? args[2] : "YouTube Music",
                    args.Length > 3 ? args[3] : "");
    return;
}

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseUrls("http://localhost:5800");
var app = builder.Build();
app.UseDefaultFiles();
app.UseStaticFiles();

var mgr = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();

// ponytail: 「現在のセッション」1つだけ見る。複数アプリが鳴るとき YouTube Music を選ぶ処理は未実装
app.MapGet("/api/now", async () =>
{
    var s = mgr.GetCurrentSession();
    if (s is null) return Results.Json(new { playing = false });
    var p = await s.TryGetMediaPropertiesAsync();
    var playing = s.GetPlaybackInfo().PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing;
    return Results.Json(new { playing, title = p.Title, artist = p.Artist, app = s.SourceAppUserModelId });
});

app.MapGet("/api/thumb", async () =>
{
    var p = await (mgr.GetCurrentSession()?.TryGetMediaPropertiesAsync() ?? throw new InvalidOperationException());
    if (p.Thumbnail is null) return Results.NotFound();
    using var stream = await p.Thumbnail.OpenReadAsync();
    using var ms = new MemoryStream();
    await stream.AsStreamForRead().CopyToAsync(ms);
    return Results.File(ms.ToArray(), stream.ContentType);
});

app.Run();
