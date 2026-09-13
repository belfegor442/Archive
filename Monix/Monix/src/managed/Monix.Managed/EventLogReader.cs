using System.Diagnostics.Eventing.Reader;

namespace Monix.Managed;

public class MonixEventLogReader : IDisposable
{
    private readonly List<EventLogWatcher> _watchers = new();
    private bool _disposed;

    public event EventHandler<EventRecordWrittenEventArgs>? EventReceived;

    public MonixEventLogReader()
    {
    }

    public List<EventRecord> ReadEvents(string logName, int maxEvents = 100, bool fromOldest = false)
    {
        var events = new List<EventRecord>();

        var query = new EventLogQuery(logName, PathType.LogName)
        {
            ReverseDirection = !fromOldest
        };

        using var reader = new System.Diagnostics.Eventing.Reader.EventLogReader(query);
        EventRecord? record;
        while ((record = reader.ReadEvent()) != null && events.Count < maxEvents)
        {
            events.Add(record);
        }

        return events;
    }

    public List<EventRecord> ReadEventsSince(string logName, DateTime since, int maxEvents = 1000)
    {
        var events = new List<EventRecord>();

        var query = new EventLogQuery(logName, PathType.LogName)
        {
            ReverseDirection = true
        };

        using var reader = new System.Diagnostics.Eventing.Reader.EventLogReader(query);
        EventRecord? record;
        while ((record = reader.ReadEvent()) != null && events.Count < maxEvents)
        {
            if (record.TimeCreated >= since)
            {
                events.Add(record);
            }
        }

        return events;
    }

    public void Subscribe(string logName)
    {
        var query = new EventLogQuery(logName, PathType.LogName);

        var watcher = new EventLogWatcher(query);
        watcher.EventRecordWritten += OnEventRecordWritten;
        watcher.Enabled = true;
        _watchers.Add(watcher);
    }

    public void UnsubscribeAll()
    {
        foreach (var watcher in _watchers)
        {
            watcher.Enabled = false;
            watcher.EventRecordWritten -= OnEventRecordWritten;
            watcher.Dispose();
        }
        _watchers.Clear();
    }

    private void OnEventRecordWritten(object? sender, EventRecordWrittenEventArgs e)
    {
        if (e.EventRecord != null)
        {
            EventReceived?.Invoke(this, e);
        }
    }

    public static Dictionary<string, object> EventToDict(EventRecord record)
    {
        return new Dictionary<string, object>
        {
            ["Id"] = record.Id,
            ["LevelDisplayName"] = record.LevelDisplayName ?? "",
            ["Message"] = record.FormatDescription() ?? "",
            ["ProviderName"] = record.ProviderName ?? "",
            ["TimeCreated"] = record.TimeCreated?.ToString("o") ?? "",
            ["MachineName"] = record.MachineName ?? "",
            ["UserId"] = record.UserId?.ToString() ?? "",
            ["ProcessId"] = record.ProcessId ?? 0,
            ["ThreadId"] = record.ThreadId ?? 0,
            ["OpcodeDisplayName"] = record.OpcodeDisplayName ?? "",
            ["TaskDisplayName"] = record.TaskDisplayName ?? "",
        };
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        UnsubscribeAll();
    }
}