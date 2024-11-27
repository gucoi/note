/*
 * @lc app=leetcode.cn id=404 lang=cpp
 *
 * [404] 左叶子之和
 */

// @lc code=start
/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *     int val;
 *     TreeNode *left;
 *     TreeNode *right;
 *     TreeNode() : val(0), left(nullptr), right(nullptr) {}
 *     TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
 *     TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
 * };
 */
class Solution {
public:
    int flag = 0;
    int dfs(TreeNode* root, int flag) {
        if (!root) return 0;
        if (!root->left && !root->right && flag) {
            return root->val;
        }
        return dfs(root->left, flag | 1) + dfs(root->right, flag & 0);
    }
    int sumOfLeftLeaves(TreeNode* root) {
        return dfs(root, flag);
    }
};
// @lc code=end

