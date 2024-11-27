/*
 * @lc app=leetcode.cn id=785 lang=cpp
 *
 * [785] 判断二分图
 */

// @lc code=start
class Solution {
public:
    vector<int> p;
    int find(int x) {
        if (p[x] != x) p[x] = find(p[x]);
        return p[x];
    }

    bool isBipartite(vector<vector<int>>& graph) {
        p.resize(graph.size());

        for (int i = 0; i < graph.size(); i ++) {
            p[i] = i;
        }

        for (int i = 0; i < graph.size(); i ++) {
            if (graph[i].size() == 0) continue;

            int x = find(i);
            int y = find(graph[i][0]);

            if (x == y) return false;

            for (int j = 1; j < graph[i].size(); j ++) {
                int z = find(graph[i][j]);
                if (x == z) return false;
                p[z] = y;
            }
        }

        return true;
    }
};
// @lc code=end

