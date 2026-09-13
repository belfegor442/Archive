using System.Management;

namespace Monix.Managed;

public class PnPDevice
{
    public string DeviceId { get; set; } = "";
    public string Name { get; set; } = "";
    public string Description { get; set; } = "";
    public string Manufacturer { get; set; } = "";
    public string ClassGuid { get; set; } = "";
    public string ClassName { get; set; } = "";
    public string Status { get; set; } = "";
    public DateTime? InstallDate { get; set; }
    public bool IsPresent { get; set; }
    public List<string> HardwareIds { get; set; } = new();
    public List<string> CompatibleIds { get; set; } = new();
}

public class PnPChangeEventArgs : EventArgs
{
    public List<PnPDevice> AddedDevices { get; set; } = new();
    public List<PnPDevice> RemovedDevices { get; set; } = new();
    public List<PnPDevice> ChangedDevices { get; set; } = new();
}

public class PnPMonitor : IDisposable
{
    private readonly WmiConnector _wmi;
    private ManagementEventWatcher? _arrivalWatcher;
    private ManagementEventWatcher? _removalWatcher;
    private Dictionary<string, PnPDevice> _knownDevices = new();
    private bool _disposed;

    public event EventHandler<PnPChangeEventArgs>? DevicesChanged;

    public PnPMonitor(WmiConnector? wmi = null)
    {
        _wmi = wmi ?? new WmiConnector();
    }

    public List<PnPDevice> EnumerateDevices()
    {
        var devices = new List<PnPDevice>();
        var scope = _wmi.GetScope("cimv2");

        var query = new ObjectQuery("SELECT * FROM Win32_PnPEntity");
        using var searcher = new ManagementObjectSearcher(scope, query);
        using var collection = searcher.Get();

        foreach (ManagementObject obj in collection)
        {
            var device = new PnPDevice
            {
                DeviceId = obj["DeviceID"]?.ToString() ?? "",
                Name = obj["Name"]?.ToString() ?? "",
                Description = obj["Description"]?.ToString() ?? "",
                Manufacturer = obj["Manufacturer"]?.ToString() ?? "",
                ClassGuid = obj["ClassGuid"]?.ToString() ?? "",
                ClassName = obj["PNPClass"]?.ToString() ?? "",
                Status = obj["Status"]?.ToString() ?? "",
                IsPresent = obj["Present"] is true,
            };

            if (obj["InstallDate"] is DateTime installDate)
                device.InstallDate = installDate;

            if (obj["HardwareID"] is string[] hwIds)
                device.HardwareIds = hwIds.ToList();

            if (obj["CompatibleID"] is string[] compatIds)
                device.CompatibleIds = compatIds.ToList();

            devices.Add(device);
        }

        return devices;
    }

    public void StartMonitoring()
    {
        var scope = _wmi.GetScope("cimv2");

        var arrivalQuery = new WqlEventQuery("SELECT * FROM __InstanceCreationEvent WITHIN 2 WHERE TargetInstance ISA 'Win32_PnPEntity'");
        _arrivalWatcher = new ManagementEventWatcher(scope, arrivalQuery);
        _arrivalWatcher.EventArrived += OnDeviceArrival;
        _arrivalWatcher.Start();

        var removalQuery = new WqlEventQuery("SELECT * FROM __InstanceDeletionEvent WITHIN 2 WHERE TargetInstance ISA 'Win32_PnPEntity'");
        _removalWatcher = new ManagementEventWatcher(scope, removalQuery);
        _removalWatcher.EventArrived += OnDeviceRemoval;
        _removalWatcher.Start();
    }

    public void StopMonitoring()
    {
        _arrivalWatcher?.Stop();
        _removalWatcher?.Stop();
        _arrivalWatcher?.Dispose();
        _removalWatcher?.Dispose();
        _arrivalWatcher = null;
        _removalWatcher = null;
    }

    private void OnDeviceArrival(object sender, EventArrivedEventArgs e)
    {
        var targetInstance = e.NewEvent["TargetInstance"] as ManagementBaseObject;
        if (targetInstance == null) return;

        var device = ExtractDevice(targetInstance);
        var args = new PnPChangeEventArgs { AddedDevices = { device } };
        DevicesChanged?.Invoke(this, args);
    }

    private void OnDeviceRemoval(object sender, EventArrivedEventArgs e)
    {
        var targetInstance = e.NewEvent["TargetInstance"] as ManagementBaseObject;
        if (targetInstance == null) return;

        var device = ExtractDevice(targetInstance);
        var args = new PnPChangeEventArgs { RemovedDevices = { device } };
        DevicesChanged?.Invoke(this, args);
    }

    private static PnPDevice ExtractDevice(ManagementBaseObject obj)
    {
        return new PnPDevice
        {
            DeviceId = obj["DeviceID"]?.ToString() ?? "",
            Name = obj["Name"]?.ToString() ?? "",
            Description = obj["Description"]?.ToString() ?? "",
            Manufacturer = obj["Manufacturer"]?.ToString() ?? "",
            ClassGuid = obj["ClassGuid"]?.ToString() ?? "",
            ClassName = obj["PNPClass"]?.ToString() ?? "",
            Status = obj["Status"]?.ToString() ?? "",
            IsPresent = obj["Present"] is true,
        };
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        StopMonitoring();
        _knownDevices.Clear();
    }
}