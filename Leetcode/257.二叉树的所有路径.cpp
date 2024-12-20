/*
 * @lc app=leetcode.cn id=257 lang=cpp
 *
 * [257] 二叉树的所有路径
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
    vector<string> binaryTreePaths(TreeNode* root) {
        if (!root) return {};
        
        if (!root->left && !root->right) return {to_string(root->val)};

        vector<string> left(binaryTreePaths(root->left));
        vector<string> right(binaryTreePaths(root->right));
        
        left.insert(left.end(), right.begin(), right.end());

        for (auto &e : left) {
            e = to_string(root->val) + "->" + e;
        }
        
        return left;
    }
};
// @lc code=end

