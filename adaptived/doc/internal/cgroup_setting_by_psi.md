## cgroup_setting_by_psi Effect

The cgroup_setting_by_psi effect can be used to modify a cgroup with the
highest/lowest PSI value in a cgroup hierarchy.

### Example Use Case

Consider the following use case:
* A system runs two workloads, a high-priority workload and a low-priority
  workload
* The high-priority workload should consume up to 90% of the machine when
  necessary
* The low-priority workload should be allowed to consume extra resources (above
  and beyond its minimum 10%) when the high-priority workload doesn't need them
* The high-priority workload is located in the highpriority.slice cgroup, and
  the low-priority workload is in the low-priority cgroup

The cpu.weight setting in cgroup v2 already allows for the desired CPU
allocation.  By setting highpriority.slice's cpu.weight to 900 and
lowpriority.slice's cpu.weight to 100, the system can be divided up in the
90%/10% allocation outlined above.  If the high-priority workload doesn't
need its CPU allocation, then the low-priority workload can utilize those
CPU resources

The cgroup memory controller doesn't provide an analogous mechanism to
cpu.weight for proportionally allocating memory, but we can use adaptived to
dynamically manage the memory.

Consider the following adaptived JSON configuration:

```
{
  "rules": [
    {
      "name": "When the high priority cgroup is under pressure, induce memory reclaim in the low priority cgroup (or one of its children)",
      "causes": [
        {
          "name": "pressure",
          "args": {
            "pressure_file": "/sys/fs/cgroup/highpriority.slice/memory.pressure",
            "threshold": 5.0,
            "duration": 2,
            "measurement": "full-avg10"
          }
        }
      ],
      "effects": [
        {
          "name": "cgroup_setting_by_psi",
          "args": {
            "cgroup": "/sys/fs/cgroup/lowpriority.slice*",
            "type": "memory",
            "measurement": "full-avg10",
            "pressure_operator": "lessthan",
            "setting": "memory.high",
            "value": 1073741824,
            "setting_operator": "subtract",
            "limit": 268435456
          }
        }
      ]
    }
  ]
}
```

By monitoring PSI pressure on the high priority workload, the above rule can then reduce memory.high in the low priority cgroup(s) that are causing memory pressure.  A cgroup that is using more memory than its memory.high setting will be more likely to undergo memory reclaim when the system has memory pressure.

To detect memory pressure, this rule is using the [pressure](../../src/causes/pressure.c) cause.  When the PSI measurement is above the specified threshold for the specified duration, then this cause will trigger.
* threshold (PSI value) - PSI value to trigger on.  In this case, if the PSI value is expected to exceed this threshold in `advanced_warning` seconds in the future, then the cause will trigger
* duration (int) - Consecutive seconds that the PSI measurement must exceed the threshold for this cause to trigger
* measurement (string) - which PSI measurement to monitor.  This example has chosen to monitor the `full avg60` PSI measurement.  Other workloads may want to monitor the some, 10-second or 300-second PSI values.
* operator (string) - Either "greaterthan" or "lessthan"

When the pressure cause triggers, the [cgroup_setting_by_psi](../../src/effects/cgroup_setting_by_psi.c) effect is run.  This effect will walk the specified cgroup tree, find the cgroup with the lowest PSI usage, and then decrease that cgroup's memory.high setting.  The goal is to free up memory for the high priority cgroup while minimizing the impact on the low priority cgroup.  The settings for the cgroup_settings_by_psi effect are as follows:
* cgroup (string) - Which cgroup (or cgroup tree) to operate on
* type (string) - memory, cpu, or io.  Which PSI setting to evaluate
* measurement (string) - which PSI measurement to monitor.  This example has chosen to monitor the `full avg60` PSI measurement.  Other workloads may want to monitor the some, 10-second or 300-second PSI values.
* pressure_operator (string) - lessthan or greaterthan.  If lessthan is specified, then the cgroup with the lowest PSI will be operated upon
* setting (string) - which cgroup setting to modify after the candidate cgroup has been identified
* value (string, float, or int) - If add or subtract is specified in the `setting_operator`, the `value` will be added or subtracted, respectively, to the current value in the setting.  If a `limit` is provided and this cgroup is already at its limit, then another candidate cgroup will be operated on
* setting_operator (string) - add, subtract, or set.  If set is specified, then the `value` will be written directly to the `setting` file
* limit (float or int) - Limit the setting file to not exceed this value

The above rule will ensure memory is available to the high priority cgroup when needed, but we also need a rule to then relax the memory restrictions when the high priority cgroup is no longer putting the system under memory pressure.

```
{
  "rules": [
    {
      "name": "Reduce memory pressure on the low priority cgroup when the high priority cgroup no longer needs the memory",
      "causes": [
        {
          "name": "pressure",
          "args": {
            "pressure_file": "/sys/fs/cgroup/highpriority.slice/memory.pressure",
            "threshold": 1.0,
            "duration": 5,
            "operator": "lessthan",
            "measurement": "full-avg10"
          }
        }
      ],
      "effects": [
        {
          "name": "cgroup_setting_by_psi",
          "args": {
            "cgroup": "/sys/fs/cgroup/lowpriority.slice*",
            "type": "memory",
            "measurement": "full-avg10",
            "pressure_operator": "greaterthan",
            "setting": "memory.high",
            "value": 1073741824,
            "setting_operator": "add",
            "limit": 17179869184
          }
        }
      ]
    }
  ]
}
```

The above rule will slowly remove the memory pressure from the low priority cgroup.  Once the high priority cgroup's `full avg10` PSI memory pressure has dropped below `1.00` for five seconds, then 1 GB will be added to a cgroup in the low priority slice.  If in five seconds, the high priority PSI pressure remains below the `2.00` threshold, then again 1 GB will be added to a cgroup in the low priority slice.
