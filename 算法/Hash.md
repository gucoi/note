
## 连续数组

```c++
    int findMaxLength(vector<int>& nums) {
        int res = 0;
        unordered_map<int, int> mp;
        int counter = 0;
        mp[counter] = -1;
        for (int i = 0; i < nums.size(); i ++) {
            if (nums[i] == 1) {
                counter ++;
            } else {
                counter --;
            }
            if (mp.count(counter)) {
                res = max(res, i - mp[counter]);
            } else {
                mp[counter] = i;
            }
        }
        return res;
    }
```