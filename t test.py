import numpy as np
from scipy import stats

# Data for no slave processe (single machine)
no_slave_data = {
    1: [2313, 2326, 2339, 2321, 2421],
    2: [1494, 1505, 1470, 1498, 1526],
    4: [867, 860, 856, 887, 862],
    8: [702, 702, 709, 706, 704],
    16: [681, 655, 650, 632, 647],
    32: [643, 631, 711, 628, 644],
    64: [642, 635, 640, 640, 649],
    128: [623, 631, 630, 621, 641],
    256: [617, 620, 611, 624, 613],
    512: [609, 600, 606, 601, 600],
    1024: [594, 602, 597, 599, 591],
    2048: [600, 610, 599, 599, 601]
}

# Data for distributed model (from master.cpp)
distributed_data = {
    1: [2247, 2265, 2246, 2247, 2276],
    2: [2248, 2251, 2255, 2250, 2263],
    4: [2289, 2278, 2285, 2265, 2287],
    8: [2245, 2253, 2215, 2275, 2257],
    16: [2322, 2385, 2353, 2348, 2394],
    32: [2527, 2380, 2438, 2411, 2389],
    64: [2372, 2439, 2395, 2388, 2413],
    128: [2416, 2357, 2356, 2343, 2416],
    256: [2430, 2450, 2382, 2396, 2399],
    512: [2460, 2507, 2472, 2488, 2470],
    1024: [2507, 2517, 2506, 2567, 2647],
    2048: [2540, 2541, 2567, 2521, 2550]
}

# Data for distributed model (from slave.cpp)
distributed_slave_data = {
    1: [926, 925, 925, 927, 955],
    2: [943, 945, 946, 942, 953],
    4: [979, 983, 963, 986, 986],
    8: [946, 945, 910, 973, 954],
    16: [1019, 1086, 1053, 1047, 1094],
    32: [1112, 1080, 1118, 1109, 1089],
    64: [1073, 1140, 1093, 1086, 1097],
    128: [1117, 1050, 1053, 1042, 1112],
    256: [1130, 1151, 1074, 1094, 1097],
    512: [1158, 1199, 1171, 1182, 1169],
    1024: [1283, 1215, 1204, 1256, 1214],
    2048: [1236, 1234, 1261, 1210, 1237]
}

# mean execution times for each thread count
mean_no_slave = [np.mean(data) for data in no_slave_data.values()]
mean_distributed = [np.mean(data) for data in distributed_data.values()]

# differences in mean execution times
differences = [distributed - no_slave for distributed, no_slave in zip(mean_distributed, mean_no_slave)]

# paired t-test
t_stat, p_value = stats.ttest_rel(mean_distributed, mean_no_slave)

print("Mean Execution Times (No Slave):", mean_no_slave)
print("Mean Execution Times (Distributed):", mean_distributed)
print("Differences in Mean Execution Times:", differences)
print("Paired t-test Results:")
print("  t-statistic:", t_stat)
print("  p-value:", p_value)

alpha = 0.05
if p_value < alpha:
    print("Reject Null Hypothesis: There is a significant difference in mean execution times.")
else:
    print("Fail to Reject Null Hypothesis: There is no significant difference in mean execution times.")
