using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

// obs-websocket v5 の最小クライアント。標準の ClientWebSocket だけで書く。
class ObsClient : IDisposable
{
    readonly ClientWebSocket ws = new();
    int nextId;

    public async Task Connect(string url, string password)
    {
        await ws.ConnectAsync(new Uri(url), default);
        var hello = await Recv(); // op 0
        var d = new JsonObject { ["rpcVersion"] = 1, ["eventSubscriptions"] = 0 };
        if (hello["d"]?["authentication"] is JsonNode a)
        {
            var secret = Sha64(password + (string)a["salt"]!);
            d["authentication"] = Sha64(secret + (string)a["challenge"]!);
        }
        await Send(new JsonObject { ["op"] = 1, ["d"] = d });
        await Recv(); // op 2 Identified。認証失敗なら接続が閉じられ、ここで例外になる
    }

    public async Task<JsonNode> Request(string type, JsonObject? data = null)
    {
        var id = (++nextId).ToString();
        var d = new JsonObject { ["requestType"] = type, ["requestId"] = id };
        if (data is not null) d["requestData"] = data;
        await Send(new JsonObject { ["op"] = 6, ["d"] = d });
        while (true)
        {
            var m = await Recv();
            if ((int)m["op"]! != 7 || (string)m["d"]!["requestId"]! != id) continue;
            var status = m["d"]!["requestStatus"]!;
            if (!(bool)status["result"]!) throw new Exception($"{type} 失敗: {status}");
            return m["d"]!["responseData"] ?? new JsonObject();
        }
    }

    static string Sha64(string s) => Convert.ToBase64String(SHA256.HashData(Encoding.UTF8.GetBytes(s)));

    Task Send(JsonNode n) =>
        ws.SendAsync(Encoding.UTF8.GetBytes(n.ToJsonString()), WebSocketMessageType.Text, true, default);

    async Task<JsonNode> Recv()
    {
        var buf = new byte[64 * 1024];
        using var ms = new MemoryStream();
        WebSocketReceiveResult r;
        do
        {
            r = await ws.ReceiveAsync(buf, default);
            if (r.MessageType == WebSocketMessageType.Close)
                throw new Exception($"OBS が接続を閉じた: {r.CloseStatusDescription}");
            ms.Write(buf, 0, r.Count);
        } while (!r.EndOfMessage);
        return JsonNode.Parse(ms.ToArray())!;
    }

    public void Dispose() => ws.Dispose();
}

// 実験用: OBS に接続して、入力の一覧とウィンドウキャプチャの設定・ウィンドウ候補をそのまま表示する。
static class Probe
{
    public static async Task Run(string password)
    {
        using var obs = new ObsClient();
        await obs.Connect("ws://localhost:4455", password);
        var list = await obs.Request("GetInputList");
        foreach (var i in list["inputs"]!.AsArray())
        {
            var name = (string)i!["inputName"]!;
            var kind = (string)i["inputKind"]!;
            Console.WriteLine($"- {name} ({kind})");
            if (kind != "window_capture") continue;
            var req = () => new JsonObject { ["inputName"] = name };
            Console.WriteLine("  settings: " + (await obs.Request("GetInputSettings", req()))["inputSettings"]);
            var q = req(); q["propertyName"] = "window";
            Console.WriteLine("  windows: " + (await obs.Request("GetInputPropertiesListPropertyItems", q))["propertyItems"]);
        }
    }
}

// 見張り: 保存中のウィンドウが一覧から消えたら（itemEnabled=false の項目が出る）、keyword を含む候補に付け替える。
static class Watch
{
    public static async Task Run(string input, string keyword, string password)
    {
        using var obs = new ObsClient();
        await obs.Connect("ws://localhost:4455", password);
        Console.WriteLine($"見張り開始: 「{input}」 を「{keyword}」に合わせる");
        string? lostLogged = null;
        while (true)
        {
            var q = new JsonObject { ["inputName"] = input, ["propertyName"] = "window" };
            var items = (await obs.Request("GetInputPropertiesListPropertyItems", q))["propertyItems"]!.AsArray();
            var lost = items.FirstOrDefault(i => !(bool)i!["itemEnabled"]!);
            if (lost is null) { lostLogged = null; }
            else
            {
                var lostValue = (string)lost["itemValue"]!;
                var hit = items.FirstOrDefault(i => (bool)i!["itemEnabled"]!
                    && ((string)i["itemName"]!).Contains(keyword, StringComparison.OrdinalIgnoreCase));
                if (hit is not null)
                {
                    var set = new JsonObject
                    {
                        ["inputName"] = input,
                        ["inputSettings"] = new JsonObject { ["window"] = (string)hit["itemValue"]! },
                    };
                    await obs.Request("SetInputSettings", set);
                    Console.WriteLine($"{DateTime.Now:T} 付け替え: {lostValue} -> {hit["itemValue"]}");
                    lostLogged = null;
                }
                else if (lostLogged != lostValue)
                {
                    lostLogged = lostValue;
                    Console.WriteLine($"{DateTime.Now:T} 見失った: {lostValue}（候補なし。chrome.exe の窓:）");
                    foreach (var i in items.Where(i => ((string)i!["itemName"]!).StartsWith("[chrome.exe]")))
                        Console.WriteLine("    " + i!["itemName"]);
                }
            }
            await Task.Delay(2000);
        }
    }
}
