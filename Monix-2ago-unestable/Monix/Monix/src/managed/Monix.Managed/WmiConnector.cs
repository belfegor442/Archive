using System.Management;

namespace Monix.Managed;

public class WmiConnector : IDisposable
{
    private readonly Dictionary<string, ManagementScope> _scopes = new();
    private bool _disposed;

    public WmiConnector()
    {
        ConnectToLocalMachine();
    }

    private void ConnectToLocalMachine()
    {
        var scope = new ManagementScope(@"\\.\root\cimv2");
        scope.Connect();
        _scopes["cimv2"] = scope;
    }

    public ManagementScope GetScope(string namespaceName = "cimv2")
    {
        if (_scopes.TryGetValue(namespaceName, out var scope))
            return scope;

        scope = new ManagementScope($@"\\.\root\{namespaceName}");
        scope.Connect();
        _scopes[namespaceName] = scope;
        return scope;
    }

    public List<Dictionary<string, object>> Query(string wql, string namespaceName = "cimv2")
    {
        var results = new List<Dictionary<string, object>>();
        var scope = GetScope(namespaceName);

        using var searcher = new ManagementObjectSearcher(scope, new ObjectQuery(wql));
        using var collection = searcher.Get();

        foreach (ManagementObject obj in collection)
        {
            var dict = new Dictionary<string, object>();
            foreach (PropertyData prop in obj.Properties)
            {
                dict[prop.Name] = prop.Value ?? DBNull.Value;
            }
            results.Add(dict);
        }

        return results;
    }

    public Dictionary<string, object>? QueryFirst(string wql, string namespaceName = "cimv2")
    {
        var results = Query(wql, namespaceName);
        return results.Count > 0 ? results[0] : null;
    }

    public List<Dictionary<string, object>> GetCpuInfo()
    {
        return Query("SELECT * FROM Win32_Processor");
    }

    public List<Dictionary<string, object>> GetMemoryInfo()
    {
        return Query("SELECT * FROM Win32_OperatingSystem");
    }

    public List<Dictionary<string, object>> GetDiskInfo()
    {
        return Query("SELECT * FROM Win32_LogicalDisk WHERE DriveType=3");
    }

    public List<Dictionary<string, object>> GetNetworkInfo()
    {
        return Query("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=TRUE");
    }

    public List<Dictionary<string, object>> GetProcessList()
    {
        return Query("SELECT ProcessId, Name, WorkingSetSize, KernelModeTime, UserModeTime FROM Win32_Process");
    }

    public List<Dictionary<string, object>> GetServiceList()
    {
        return Query("SELECT Name, State, StartMode, ProcessId FROM Win32_Service");
    }

    public List<Dictionary<string, object>> GetEventLogs(string logName = "System", int maxEvents = 100)
    {
        return Query($"SELECT TOP {maxEvents} * FROM Win32_NTLogEvent WHERE LogFile='{logName}' ORDER BY TimeGenerated DESC");
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _scopes.Clear();
    }
}