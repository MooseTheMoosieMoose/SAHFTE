%% Data Points
object_count = [10, 20, 50, 100, 500, 1000, 2000, 10000, 50000];
time_ms = [
    0.46982765197753906,
    0.4419565200805664,
    0.49262046813964844,
    0.5270481109619141,
    0.8659124374389648,
    1.4908790588378906,
    2.694272994995117,
    12.754344940185547,
    72.8672981262207
    ];

time_ms_v3 = [
    0.5235433578491211,
    0.6910324096679688,
    1.0100364685058594,
    1.7795801162719727,
    7.347822189331055,
    13.618087768554688,
    27.005934715270996,
    131.9575309753418,
    688.2725715637207
];

time_ms_v3_64 = [
    0.5468368530273438,
    0.7409811019897461,
    1.0438203811645508,
    1.7334938049316406,
    6.743001937866211,
    12.752699851989746,
    24.6243953704834,
    122.52035140991211,
    638.6480331420898
];

time_ms_v3_opt = [
    0.2767801284790039,
    0.2727508544921875,
    0.2350330352783203,
    0.40450096130371094,
    1.1384963989257812,
    1.9511699676513672,
    3.638744354248047,
    17.246580123901367,
    97.72083759307861
];

%% Plot
plot(object_count, time_ms, "DisplayName", "V3 Unstable");

hold on;

plot(object_count, time_ms_v3, "DisplayName", "V3 Stable C=16");

hold on;

plot(object_count, time_ms_v3_64, "DisplayName", "V3 Stable C=64");

hold on;

plot(object_count, time_ms_v3_opt, "DisplayName", "V3 Stable C=64, No CV");

grid on;
title("Fusion Time (Python Bindings)")
subtitle("Runs Per OC = 10, AT = 3, SPD = 8, Coords = Local");
ylabel("Time (ms)")
xlabel("Objects in Scene")
legend("show");


figure;
plot(object_count, time_ms_v3_opt);
grid on;
title("Fusion Time (Python Bindings)")
subtitle("Runs Per OC = 10, AT = 3, SPD = 8, Coords = Local");
ylabel("Time (ms)")
xlabel("Objects in Scene")