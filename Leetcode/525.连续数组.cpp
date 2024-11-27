/*
 * @lc app=leetcode.cn id=525 lang=cpp
 *
 * [525] 连续数组
 */

// @lc code=start
class Solution {
public:
    int findMaxLength(vector<int>& nums) {
        int res = 0;
        unordered_map<int, int> mp;
        int counter = 0;
        mp[counter] = 0;
        for (int i = 0; i < nums.size(); i ++) {
            if (nums[i] == 1) {
                counter ++;
            } else {
                counter --;
            }
            if (mp.count(counter)) {
                res = max(res, i - mp[counter] + 1);
            }

            mp[counter] = i;
        }
        return res;
    }
};
// @lc code=end

