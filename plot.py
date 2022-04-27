import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
    data = pd.read_csv("perf.csv")
    dic = {}
    for i in range(len(data)):
        key = data.loc[i, "Bytes"]
        value = data.loc[i, "Time"]
        if not dic.get(key):
            dic[key] = (1, value)
        else:
            dic[key] = (dic.get(key)[0]+1, dic.get(key)[1]+value)
    temp_tab = []
    for key in dic.keys():
        val = dic.get(key)
        dic[key] = val[1]/val[0]
        temp_tab.append((key, dic.get(key)))
    temp_tab.sort()
    x_tab = [elem[0] for elem in temp_tab]
    y_tab = [elem[1] for elem in temp_tab]
    fig, ax = plt.subplots()
    ax.plot(x_tab, y_tab, linewidth=2.0)
    ax.set_title("Perfect network transfer simulation")
    ax.set_ylabel("Time (s)")
    ax.set_xlabel("Number of bytes")
    plt.show()
