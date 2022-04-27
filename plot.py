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
            if not value <= 0.5*dic.get(key)[0]/dic.get(key)[1]:  # escape bullshit  values
                dic[key] = (dic.get(key)[0]+1, dic.get(key)[1]+value)
    x_tab, y_tab = [], []
    for key in dic.keys():
        val = dic.get(key)
        dic[key] = val[1]/val[0]
        y_tab.append(dic.get(key))
        x_tab.append(key)
    print(x_tab)
    print(y_tab)
    fig, ax = plt.subplots()
    ax.plot(x_tab, y_tab, linewidth=2.0)
    plt.show()
