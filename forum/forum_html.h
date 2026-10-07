#pragma once
#include <string>

inline const char* FORUM_HTML = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Next 开放论坛</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#0a0a1a;--card:#12122e;--card2:#1a1a3e;--border:#2a2a4e;--primary:#8B8CF0;--primary-dim:#4A4B8C;--text:#e0e0f0;--text-dim:#8888aa;--accent:#6C6DF0;--danger:#F06080;--success:#60F080;--warn:#F0C060}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,'Segoe UI',sans-serif;line-height:1.6;min-height:100vh}
/* 导航栏 */
nav{background:var(--card);border-bottom:1px solid var(--border);padding:0 20px;display:flex;align-items:center;gap:15px;height:60px;position:sticky;top:0;z-index:100}
.logo{display:flex;align-items:center;gap:8px;font-size:1.3em;font-weight:bold;color:var(--primary);cursor:pointer}
.logo svg{width:28px;height:28px}
nav .nav-links{display:flex;gap:5px;flex:1}
nav a{color:var(--text-dim);text-decoration:none;padding:8px 12px;border-radius:6px;transition:all .2s;cursor:pointer;font-size:.95em}
nav a:hover{color:var(--text);background:var(--border)}
nav a.active{color:var(--primary);background:rgba(139,140,240,.1)}
/* 搜索框 */
.search-box{display:flex;align-items:center;gap:8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;padding:6px 12px;width:220px}
.search-box input{background:transparent;border:none;color:var(--text);outline:none;width:100%;font-size:.9em}
.search-box svg{width:16px;height:16px;color:var(--text-dim);flex-shrink:0}
/* 用户区域 */
.user-area{display:flex;align-items:center;gap:10px}
.user-avatar{width:32px;height:32px;border-radius:50%;border:2px solid var(--primary)}
.user-name{color:var(--text);font-size:.9em}
.btn-login{color:var(--primary);cursor:pointer;font-size:.9em;padding:6px 14px;border:1px solid var(--primary);border-radius:6px;transition:all .2s}
.btn-login:hover{background:rgba(139,140,240,.1)}
/* 主内容 */
main{max-width:1200px;margin:0 auto;padding:30px 20px}
section{display:none}
section.active{display:block}
/* 首页 */
.hero{text-align:center;padding:60px 20px;background:linear-gradient(135deg,var(--card),var(--bg));border-radius:16px;margin-bottom:30px;border:1px solid var(--border)}
.hero svg{width:80px;height:80px;margin-bottom:20px}
.hero h1{font-size:2.5em;margin-bottom:10px;background:linear-gradient(135deg,var(--primary),var(--accent));-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.hero p{color:var(--text-dim);font-size:1.1em;max-width:600px;margin:0 auto 30px}
.hero-btns{display:flex;gap:15px;justify-content:center;flex-wrap:wrap}
/* 统计卡片 */
.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:30px}
.stat-card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;text-align:center;transition:all .2s}
.stat-card:hover{border-color:var(--primary);transform:translateY(-2px)}
.stat-card .num{font-size:2em;font-weight:bold;color:var(--primary)}
.stat-card .label{color:var(--text-dim);font-size:.9em;margin-top:5px}
/* 按钮 */
.btn{display:inline-flex;align-items:center;gap:6px;padding:10px 24px;border-radius:8px;border:none;cursor:pointer;font-size:.95em;transition:all .2s;text-decoration:none}
.btn-primary{background:var(--primary);color:#fff}
.btn-primary:hover{background:var(--accent);transform:translateY(-1px)}
.btn-primary:disabled{opacity:.5;cursor:not-allowed;transform:none}
.btn-outline{background:transparent;color:var(--primary);border:1px solid var(--primary)}
.btn-outline:hover{background:rgba(139,140,240,.1)}
.btn-danger{background:var(--danger);color:#fff}
.btn-danger:hover{opacity:.85}
.btn-success{background:var(--success);color:#000}
.btn-sm{padding:6px 14px;font-size:.85em}
.btn-xs{padding:4px 10px;font-size:.8em}
/* 卡片 */
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;margin-bottom:16px;transition:all .2s;cursor:pointer}
.card:hover{border-color:var(--primary-dim);background:var(--card2)}
.card h3{color:var(--primary);margin-bottom:8px}
.card .meta{color:var(--text-dim);font-size:.85em;margin-bottom:10px;display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.card .body{color:var(--text);white-space:pre-wrap;word-break:break-word;max-height:150px;overflow:hidden;position:relative}
.card .body::after{content:'';position:absolute;bottom:0;left:0;right:0;height:40px;background:linear-gradient(transparent,var(--card))}
.card .stats{display:flex;gap:15px;margin-top:12px;color:var(--text-dim);font-size:.85em}
.card .stats span{display:flex;align-items:center;gap:4px}
.section-title{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;flex-wrap:wrap;gap:10px}
.section-title h2{color:var(--text)}
/* 工具栏 */
.toolbar{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:20px}
.toolbar select,.toolbar input{background:var(--bg);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:8px 12px;font-size:.9em;outline:none}
.toolbar select:focus,.toolbar input:focus{border-color:var(--primary)}
/* 代码编辑器 */
.editor-wrap{display:flex;flex-direction:column;gap:15px}
.editor-toolbar{display:flex;gap:10px;align-items:center}
textarea.code-editor{width:100%;min-height:300px;background:#0d0d1e;color:var(--text);border:1px solid var(--border);border-radius:8px;padding:15px;font-family:'Consolas','Courier New',monospace;font-size:14px;resize:vertical;line-height:1.5}
textarea.code-editor:focus{outline:none;border-color:var(--primary)}
.output-box{background:#0d0d1e;border:1px solid var(--border);border-radius:8px;padding:15px;min-height:100px;font-family:'Consolas','Courier New',monospace;font-size:14px;white-space:pre-wrap;word-break:break-word}
.output-box .label{color:var(--text-dim);font-size:.85em;margin-bottom:8px}
.output-box .content{color:var(--success)}
.output-box .error{color:var(--danger)}
/* 表单 */
.form-group{margin-bottom:15px}
.form-group label{display:block;margin-bottom:6px;color:var(--text-dim);font-size:.9em}
.form-group input,.form-group textarea,.form-group select{width:100%;background:var(--bg);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:10px;font-size:.95em}
.form-group input:focus,.form-group textarea:focus,.form-group select:focus{outline:none;border-color:var(--primary)}
.form-group textarea{min-height:120px;font-family:'Consolas','Courier New',monospace;resize:vertical}
/* 模态框 */
.modal-overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.7);z-index:200;justify-content:center;align-items:center}
.modal-overlay.active{display:flex}
.modal{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:25px;width:90%;max-width:600px;max-height:85vh;overflow-y:auto}
.modal h2{color:var(--primary);margin-bottom:20px}
.modal .close{float:right;cursor:pointer;color:var(--text-dim);font-size:1.5em;line-height:1}
.modal .close:hover{color:var(--text)}
/* 帖子详情 */
.post-detail{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:30px;margin-bottom:20px}
.post-detail h1{color:var(--primary);margin-bottom:15px;font-size:1.5em}
.post-detail .meta{color:var(--text-dim);font-size:.9em;margin-bottom:20px;padding-bottom:15px;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:12px;flex-wrap:wrap}
.post-detail .body{color:var(--text);line-height:1.8;white-space:pre-wrap;word-break:break-word}
.post-detail .actions{display:flex;gap:10px;margin-top:20px;padding-top:15px;border-top:1px solid var(--border)}
/* 评论 */
.comment{background:var(--card2);border:1px solid var(--border);border-radius:8px;padding:15px;margin-bottom:12px}
.comment .meta{color:var(--text-dim);font-size:.85em;margin-bottom:8px;display:flex;align-items:center;gap:8px}
.comment .meta img{width:24px;height:24px;border-radius:50%}
.comment .body{color:var(--text);white-space:pre-wrap;word-break:break-word;line-height:1.6}
.comment-form{background:var(--card2);border:1px solid var(--border);border-radius:8px;padding:15px;margin-top:20px}
/* 加载和提示 */
.loading{text-align:center;padding:40px;color:var(--text-dim)}
.toast{position:fixed;bottom:20px;right:20px;background:var(--card);border:1px solid var(--primary);border-radius:8px;padding:12px 20px;z-index:300;transition:all .3s;opacity:0;transform:translateY(20px);max-width:400px}
.toast.show{opacity:1;transform:translateY(0)}
.toast.error{border-color:var(--danger)}
.toast.success{border-color:var(--success)}
/* 下载页 */
.download-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px}
.download-card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:25px;text-align:center}
.download-card h3{color:var(--primary);margin-bottom:10px}
.download-card p{color:var(--text-dim);margin-bottom:20px;font-size:.9em}
/* 分页 */
.pagination{display:flex;justify-content:center;gap:8px;margin-top:20px}
.pagination button{background:var(--card);border:1px solid var(--border);color:var(--text);padding:8px 16px;border-radius:6px;cursor:pointer;transition:all .2s}
.pagination button:hover{border-color:var(--primary)}
.pagination button.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.pagination button:disabled{opacity:.4;cursor:not-allowed}
/* 标签 */
.tag{display:inline-block;padding:2px 10px;border-radius:12px;font-size:.8em;margin-right:5px;cursor:pointer;transition:all .2s}
.tag:hover{opacity:.8}
.tag-discussion{background:rgba(96,240,128,.15);color:var(--success)}
.tag-bug{background:rgba(240,96,128,.15);color:var(--danger)}
.tag-library{background:rgba(139,140,240,.15);color:var(--primary)}
.tag-general{background:rgba(240,192,96,.15);color:var(--warn)}
/* 标签云 */
.tag-cloud{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:20px}
.tag-cloud .tag{font-size:1em;padding:5px 15px}
/* 空状态 */
.empty{text-align:center;padding:60px;color:var(--text-dim)}
.empty svg{width:60px;height:60px;margin-bottom:15px;opacity:.3}
/* 返回按钮 */
.back-btn{display:inline-flex;align-items:center;gap:6px;color:var(--text-dim);cursor:pointer;margin-bottom:15px;font-size:.9em}
.back-btn:hover{color:var(--primary)}
/* Markdown渲染 */
.md h1{color:var(--primary);margin:15px 0 10px;font-size:1.4em}
.md h2{color:var(--primary);margin:12px 0 8px;font-size:1.2em}
.md h3{color:var(--primary);margin:10px 0 6px;font-size:1.1em}
.md p{margin:8px 0}
.md ul,.md ol{margin:8px 0;padding-left:25px}
.md li{margin:4px 0}
.md code{background:#0d0d1e;padding:2px 6px;border-radius:4px;font-family:'Consolas',monospace;font-size:.9em;color:var(--success)}
.md pre{background:#0d0d1e;border:1px solid var(--border);border-radius:8px;padding:12px;overflow-x:auto;margin:10px 0}
.md pre code{background:none;padding:0;color:var(--text)}
.md blockquote{border-left:3px solid var(--primary);padding-left:15px;margin:10px 0;color:var(--text-dim)}
.md a{color:var(--primary);text-decoration:none}
.md a:hover{text-decoration:underline}
.md img{max-width:100%;border-radius:8px;margin:10px 0}
.md table{border-collapse:collapse;width:100%;margin:10px 0}
.md th,.md td{border:1px solid var(--border);padding:8px;text-align:left}
.md th{background:var(--card2)}
.md hr{border:none;border-top:1px solid var(--border);margin:15px 0}
/* 响应式 */
@media(max-width:768px){nav{flex-wrap:wrap;height:auto;padding:10px}nav .nav-links{order:3;width:100%}nav a{font-size:.85em;padding:6px 8px}.hero h1{font-size:1.8em}.search-box{width:150px}.stats-grid{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<nav>
<div class="logo" onclick="routeTo('home')">
<svg viewBox="0 0 100 50"><path d="M10,8 L38,25 L10,42" stroke="#8B8CF0" stroke-width="7" fill="none" stroke-linecap="round" stroke-linejoin="round"/><path d="M52,8 L80,25 L52,42" stroke="#4A4B8C" stroke-width="7" fill="none" stroke-linecap="round" stroke-linejoin="round"/></svg>
<span>Next 论坛</span>
</div>
<div class="nav-links">
<a data-route="home" class="active">首页</a>
<a data-route="download">下载</a>

<a data-route="forum">技术交流</a>
<a data-route="bugs">漏洞报告</a>
<a data-route="libs">第三方库</a>
</div>
<div class="search-box">
<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><path d="m21 21-4.35-4.35"/></svg>
<input type="text" id="search-input" placeholder="搜索帖子..." onkeydown="if(event.key==='Enter')doSearch()">
</div>
<div class="user-area" id="user-area">
<span class="btn-login" onclick="openLoginModal()">登录</span>
</div>
</nav>
<main>
<!-- 首页 -->
<section id="home" class="active">
<div class="hero">
<svg viewBox="0 0 100 50"><path d="M10,8 L38,25 L10,42" stroke="#8B8CF0" stroke-width="7" fill="none" stroke-linecap="round" stroke-linejoin="round"/><path d="M52,8 L80,25 L52,42" stroke="#4A4B8C" stroke-width="7" fill="none" stroke-linecap="round" stroke-linejoin="round"/></svg>
<h1>Next 编程语言</h1>
<p>Next 是一门简洁、高效的嵌入式脚本语言，支持函数式编程、面向对象、生成器、协程、异步IO等现代语言特性。</p>
<div class="hero-btns">
<a class="btn btn-primary" data-route="download">下载源码</a>

<a class="btn btn-outline" data-route="forum">参与讨论</a>
</div>
</div>
<div class="stats-grid" id="stats-grid"><div class="loading">加载统计中...</div></div>
<div class="section-title"><h2>最新讨论</h2><a class="btn btn-outline btn-sm" data-route="forum">查看全部</a></div>
<div id="home-posts"><div class="loading">加载中...</div></div>
</section>
<!-- 下载 -->
<section id="download">
<div class="section-title"><h2>下载 Next 解释器</h2></div>
<div class="download-grid">
<div class="download-card">
<h3>源代码</h3>
<p>从 GitHub 获取完整源码，自行编译构建</p>
<a class="btn btn-primary" href="https://github.com/Next-Apex-Dev/Next-Interpreter" target="_blank">GitHub 仓库</a>
</div>
<div class="download-card">
<h3>可执行文件</h3>
<p>下载预编译的 Next-IDE.exe，开箱即用</p>
<a class="btn btn-primary" href="https://github.com/Next-Apex-Dev/Next-Interpreter/releases" target="_blank">Releases</a>
</div>
<div class="download-card">
<h3>编译说明</h3>
<p>需要 C++20 编译器（GCC 13+ / MSVC 2022+）</p>
<a class="btn btn-outline" href="https://github.com/Next-Apex-Dev/Next-Interpreter#编译说明" target="_blank">查看文档</a>
</div>
<div class="download-card">
<h3>论坛桌面应用</h3>
<p>NextForum.exe 零依赖单文件，内置解释器</p>
<a class="btn btn-outline" href="https://github.com/Next-Apex-Dev/Next-Interpreter/releases" target="_blank">下载论坛</a>
</div>
</div>
</section>

<!-- 技术交流 -->
<section id="forum">
<div class="section-title"><h2>技术交流</h2><button class="btn btn-primary btn-sm" onclick="openPostModal('discussion')">发帖</button></div>
<div class="toolbar">
<select id="forum-sort" onchange="reloadList('forum-posts','discussion')">
<option value="created">最新创建</option>
<option value="updated">最近更新</option>
<option value="comments">最多评论</option>
</select>
<span style="color:var(--text-dim);font-size:.85em" id="forum-count"></span>
</div>
<div id="forum-posts"><div class="loading">加载中...</div></div>
<div class="pagination" id="forum-pagination"></div>
</section>
<!-- 漏洞报告 -->
<section id="bugs">
<div class="section-title"><h2>漏洞报告</h2><button class="btn btn-danger btn-sm" onclick="openPostModal('bug')">提交报告</button></div>
<div class="toolbar">
<select id="bugs-sort" onchange="reloadList('bug-posts','bug')">
<option value="created">最新创建</option>
<option value="updated">最近更新</option>
<option value="comments">最多评论</option>
</select>
<span style="color:var(--text-dim);font-size:.85em" id="bugs-count"></span>
</div>
<div id="bug-posts"><div class="loading">加载中...</div></div>
<div class="pagination" id="bugs-pagination"></div>
</section>
<!-- 第三方库 -->
<section id="libs">
<div class="section-title"><h2>第三方库</h2><button class="btn btn-primary btn-sm" onclick="openPostModal('library')">提交库</button></div>
<div class="toolbar">
<select id="libs-sort" onchange="reloadList('lib-posts','library')">
<option value="created">最新创建</option>
<option value="updated">最近更新</option>
<option value="comments">最多评论</option>
</select>
<span style="color:var(--text-dim);font-size:.85em" id="libs-count"></span>
</div>
<div id="lib-posts"><div class="loading">加载中...</div></div>
<div class="pagination" id="libs-pagination"></div>
</section>
<!-- 帖子详情 -->
<section id="detail">
<span class="back-btn" onclick="goBack()">← 返回</span>
<div id="detail-content"><div class="loading">加载中...</div></div>
</section>
<!-- 搜索结果 -->
<section id="search">
<div class="section-title"><h2>搜索结果</h2><span class="back-btn" onclick="routeTo('home')">← 返回首页</span></div>
<div id="search-results"><div class="loading">搜索中...</div></div>
</section>
</main>
<!-- 登录模态框 -->
<div class="modal-overlay" id="login-modal">
<div class="modal">
<span class="close" onclick="closeLoginModal()">&times;</span>
<h2>GitHub 登录</h2>
<div class="form-group">
<label>GitHub Personal Access Token</label>
<input type="password" id="login-token" placeholder="ghp_xxxxxxxxxxxx">
</div>
<p style="color:var(--text-dim);font-size:.85em;margin-bottom:15px">
请到 GitHub Settings > Developer settings > Personal access tokens > Tokens (classic) 生成<br>
需要 <strong>repo</strong> 权限（用于发帖、评论、点赞）<br>
Token 仅保存在浏览器本地，不会上传到服务器
</p>
<button class="btn btn-primary" id="login-btn">登录</button>
</div>
</div>
<!-- 发帖模态框 -->
<div class="modal-overlay" id="post-modal">
<div class="modal">
<span class="close" onclick="closePostModal()">&times;</span>
<h2 id="post-modal-title">发帖</h2>
<div class="form-group">
<label>标题</label>
<input type="text" id="post-title" placeholder="请输入标题（最多100字）" maxlength="100">
</div>
<div class="form-group">
<label>内容（支持 Markdown）</label>
<textarea id="post-body" placeholder="请输入内容...&#10;&#10;支持 Markdown 语法：&#10;# 标题&#10;**粗体** *斜体* `代码`&#10;```代码块```&#10;- 列表项&#10;[链接](url)"></textarea>
</div>
<div class="form-group" id="lib-url-group" style="display:none">
<label>库链接（可选）</label>
<input type="text" id="lib-url" placeholder="https://github.com/xxx/my-next-lib">
</div>
<button class="btn btn-primary" id="submit-post">提交</button>
</div>
</div>
<!-- 编辑模态框 -->
<div class="modal-overlay" id="edit-modal">
<div class="modal">
<span class="close" onclick="closeEditModal()">&times;</span>
<h2>编辑帖子</h2>
<div class="form-group">
<label>标题</label>
<input type="text" id="edit-title" maxlength="100">
</div>
<div class="form-group">
<label>内容（支持 Markdown）</label>
<textarea id="edit-body"></textarea>
</div>
<button class="btn btn-primary" id="submit-edit">保存</button>
</div>
</div>
<!-- 提示 -->
<div class="toast" id="toast"></div>
<script>
const API_BASE='';
let currentModalType='discussion';
let currentPage={};
let currentUser=null;
let userToken=localStorage.getItem('github_token')||'';
let currentDetailNumber=0;
let lastRoute='home';


// === 工具函数 ===
function showToast(msg,isError){
const t=document.getElementById('toast');
t.textContent=msg;
t.className='toast show'+(isError?' error':' success');
setTimeout(()=>t.className='toast',4000);
}
function escapeHtml(s){if(!s)return'';return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function formatDate(d){if(!d)return'';return d.substring(0,10);}
function timeAgo(d){
if(!d)return'';
const now=new Date();const then=new Date(d);
const diff=(now-then)/1000;
if(diff<60)return'刚刚';
if(diff<3600)return Math.floor(diff/60)+'分钟前';
if(diff<86400)return Math.floor(diff/3600)+'小时前';
if(diff<2592000)return Math.floor(diff/86400)+'天前';
return formatDate(d);
}

// === Markdown渲染 ===
function renderMarkdown(text){
if(!text)return'';
let html=escapeHtml(text);
// 代码块
html=html.replace(/```(\w*)\n?([\s\S]*?)```/g,(m,lang,code)=>'<pre><code>'+code+'</code></pre>');
// 标题
html=html.replace(/^### (.+)$/gm,'<h3>$1</h3>');
html=html.replace(/^## (.+)$/gm,'<h2>$1</h2>');
html=html.replace(/^# (.+)$/gm,'<h1>$1</h1>');
// 粗体和斜体
html=html.replace(/\*\*(.+?)\*\*/g,'<strong>$1</strong>');
html=html.replace(/\*(.+?)\*/g,'<em>$1</em>');
// 行内代码
html=html.replace(/`(.+?)`/g,'<code>$1</code>');
// 图片（先于链接处理）
html=html.replace(/!\[(.+?)\]\((.+?)\)/g,'<img src="$2" alt="$1">');
// 链接
html=html.replace(/\[(.+?)\]\((.+?)\)/g,'<a href="$2" target="_blank">$1</a>');
// 引用
html=html.replace(/^&gt; (.+)$/gm,'<blockquote>$1</blockquote>');
// 无序列表
html=html.replace(/^- (.+)$/gm,'<li class="ul">$1</li>');
html=html.replace(/(<li class="ul">[\s\S]*?<\/li>)/g,'<ul>$1</ul>');
// 有序列表
html=html.replace(/^\d+\. (.+)$/gm,'<li class="ol">$1</li>');
html=html.replace(/(<li class="ol">[\s\S]*?<\/li>)/g,'<ol>$1</ol>');
// 分隔线
html=html.replace(/^---$/gm,'<hr>');
// 段落（跳过pre/ul/ol/blockquote内的换行）
html=html.replace(/\n\n/g,'</p><p>');
html='<p>'+html+'</p>';
// 清理空段落和块元素外的多余p标签
html=html.replace(/<p>\s*<\/p>/g,'');
html=html.replace(/<p>\s*(<(?:ul|ol|pre|blockquote|hr)[\s\S]*?<\/(?:ul|ol|pre|blockquote)>)\s*<\/p>/g,'$1');
html=html.replace(/<p>\s*(<hr>)\s*<\/p>/g,'$1');
return '<div class="md">'+html+'</div>';
}

// === 用户管理 ===
function updateUserUI(){
const area=document.getElementById('user-area');
if(userToken){
fetch(API_BASE+'/api/user?token='+encodeURIComponent(userToken))
.then(r=>r.json()).then(data=>{
if(data.login){
currentUser=data;
localStorage.setItem('github_token',userToken);
area.innerHTML='<img class="user-avatar" src="'+escapeHtml(data.avatar_url)+'" title="'+escapeHtml(data.login)+'"><span class="user-name">'+escapeHtml(data.login)+'</span><span class="btn-login" onclick="logout()">退出</span>';
}else{
showToast('Token无效，请重新登录',true);
logout();
}
}).catch(()=>{});
}else{
area.innerHTML='<span class="btn-login" onclick="openLoginModal()">登录</span>';
}
}
function logout(){
userToken='';
currentUser=null;
localStorage.removeItem('github_token');
updateUserUI();
showToast('已退出登录');
}
function openLoginModal(){
if(userToken){logout();return;}
document.getElementById('login-token').value='';
document.getElementById('login-modal').classList.add('active');
}
function closeLoginModal(){document.getElementById('login-modal').classList.remove('active');}
document.getElementById('login-btn').addEventListener('click',()=>{
const token=document.getElementById('login-token').value.trim();
if(!token){showToast('请输入Token',true);return;}
userToken=token;
const btn=document.getElementById('login-btn');
btn.textContent='验证中...';btn.disabled=true;
fetch(API_BASE+'/api/user?token='+encodeURIComponent(token))
.then(r=>r.json()).then(data=>{
btn.textContent='登录';btn.disabled=false;
if(data.login){
currentUser=data;
localStorage.setItem('github_token',token);
updateUserUI();
closeLoginModal();
showToast('欢迎，'+data.login+'！');
}else{
showToast('Token无效',true);
userToken='';
}
}).catch(e=>{btn.textContent='登录';btn.disabled=false;showToast('网络错误',true);});
});
document.getElementById('login-modal').addEventListener('click',e=>{if(e.target.id==='login-modal')closeLoginModal();});

// === 路由 ===
function routeTo(route){
lastRoute=route;
document.querySelectorAll('section').forEach(s=>s.classList.remove('active'));
document.querySelectorAll('nav a').forEach(a=>a.classList.remove('active'));
const sec=document.getElementById(route);
if(sec)sec.classList.add('active');
const link=document.querySelector('nav a[data-route="'+route+'"]');
if(link)link.classList.add('active');
if(route==='home'){loadPosts('home-posts','discussion',5);loadStats();}
if(route==='forum')loadPosts('forum-posts','discussion');
if(route==='bugs')loadPosts('bug-posts','bug');
if(route==='libs')loadPosts('lib-posts','library');
}
function goBack(){
if(lastRoute==='detail')lastRoute='home';
routeTo(lastRoute);
}
document.querySelectorAll('[data-route]').forEach(el=>{
el.addEventListener('click',e=>{e.preventDefault();routeTo(el.dataset.route);});
});

// === 统计信息 ===
function loadStats(){
fetch(API_BASE+'/api/stats').then(r=>r.json()).then(data=>{
const grid=document.getElementById('stats-grid');
if(data.error){grid.innerHTML='';return;}
let html='';
const dc=parseInt(data.discussion_count)||0;
const bc=parseInt(data.bug_count)||0;
const lc=parseInt(data.library_count)||0;
const total=dc+bc+lc;
html+='<div class="stat-card"><div class="num">'+total+'</div><div class="label">总帖子数</div></div>';
html+='<div class="stat-card"><div class="num">'+dc+'</div><div class="label">技术讨论</div></div>';
html+='<div class="stat-card"><div class="num">'+bc+'</div><div class="label">漏洞报告</div></div>';
html+='<div class="stat-card"><div class="num">'+lc+'</div><div class="label">第三方库</div></div>';
if(data.repo){
const stars=data.repo.stargazers_count||0;
const forks=data.repo.forks_count||0;
html+='<div class="stat-card"><div class="num">'+stars+'</div><div class="label">GitHub Stars</div></div>';
html+='<div class="stat-card"><div class="num">'+forks+'</div><div class="label">Forks</div></div>';
}
grid.innerHTML=html;
}).catch(()=>{document.getElementById('stats-grid').innerHTML='';});
}

// === 帖子列表 ===
function reloadList(containerId,label){
const sortSel=document.getElementById(label==='discussion'?'forum-sort':label==='bug'?'bugs-sort':'libs-sort');
const sort=sortSel?sortSel.value:'created';
const page=currentPage[label]||1;
loadPosts(containerId,label,0,sort,page);
}
function loadPosts(containerId,label,limit,sort,page){
sort=sort||'created';
page=page||1;
currentPage[label]=page;
const c=document.getElementById(containerId);
if(!c)return;
c.innerHTML='<div class="loading">加载中...</div>';
let url=API_BASE+'/api/issues?labels='+label+'&sort='+sort+'&page='+page+'&per_page='+(limit||20);
fetch(url).then(r=>r.json()).then(data=>{
if(data.error){c.innerHTML='<div class="loading">'+escapeHtml(data.error)+'</div>';return;}
renderPosts(containerId,data,limit,label,sort,page);
}).catch(e=>{c.innerHTML='<div class="loading">网络错误，请检查连接</div>';});
}
function renderPosts(containerId,posts,limit,label,sort,page){
const c=document.getElementById(containerId);
if(!posts||posts.length===0){
c.innerHTML='<div class="empty"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 5v14H5V5h14m0-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2z"/></svg><p>暂无内容，快来发布第一条吧！</p></div>';
return;
}
let html='';
const list=limit?posts.slice(0,limit):posts;
for(const p of list){
const date=timeAgo(p.created_at);
const user=p.user?p.user.login:'unknown';
const avatar=p.user?p.user.avatar_url:'';
const comments=p.comments||0;
const labelTag='<span class="tag tag-'+label+'">'+label+'</span>';
const reactions=p.reactions||{};
const likes=reactions['+1']||0;
const hearts=reactions.heart||0;
const num=p.number||0;
html+=`<div class="card" onclick="showDetail(${num})">
<h3>${escapeHtml(p.title)}</h3>
<div class="meta">${avatar?'<img class="user-avatar" style="width:20px;height:20px" src="'+escapeHtml(avatar)+'">':''} <span>${escapeHtml(user)}</span> · <span>${date}</span> ${labelTag}</div>
<div class="body">${escapeHtml(p.body?p.body.substring(0,300):'')}</div>
<div class="stats"><span>💬 ${comments}</span><span>👍 ${likes}</span><span>❤️ ${hearts}</span></div>
</div>`;
}
c.innerHTML=html;
// 更新计数
const countId=label==='discussion'?'forum-count':label==='bug'?'bugs-count':'libs-count';
const countEl=document.getElementById(countId);
if(countEl)countEl.textContent=posts.length+' 条结果';
// 分页
if(!limit){
const pagId=label==='discussion'?'forum-pagination':label==='bug'?'bugs-pagination':'libs-pagination';
renderPagination(pagId,label,sort,page,posts.length);
}
}
function renderPagination(pagId,label,sort,page,count){
const el=document.getElementById(pagId);
if(!el)return;
if(page<=1&&count<20){el.innerHTML='';return;}
let html='';
if(page>1) html+='<button onclick="loadPosts(\''+(label==='discussion'?'forum-posts':label==='bug'?'bug-posts':'lib-posts')+'\',\''+label+'\',0,\''+sort+'\','+(page-1)+')">上一页</button>';
html+='<button class="active">'+page+'</button>';
if(count>=20) html+='<button onclick="loadPosts(\''+(label==='discussion'?'forum-posts':label==='bug'?'bug-posts':'lib-posts')+'\',\''+label+'\',0,\''+sort+'\','+(page+1)+')">下一页</button>';
el.innerHTML=html;
}

// === 帖子详情 ===
function showDetail(number){
if(!number)return;
currentDetailNumber=number;
lastRoute=document.querySelector('section.active')?document.querySelector('section.active').id:'home';
document.querySelectorAll('section').forEach(s=>s.classList.remove('active'));
document.getElementById('detail').classList.add('active');
const content=document.getElementById('detail-content');
content.innerHTML='<div class="loading">加载中...</div>';
fetch(API_BASE+'/api/issue?number='+number).then(r=>r.json()).then(post=>{
if(post.error){content.innerHTML='<div class="loading">'+escapeHtml(post.error)+'</div>';return;}
const user=post.user?post.user.login:'unknown';
const avatar=post.user?post.user.avatar_url:'';
const date=post.created_at?post.created_at.substring(0,19).replace('T',' '):'';
const reactions=post.reactions||{};
const likes=reactions['+1']||0;
const hearts=reactions.heart||0;
const labels=post.labels||[];
let labelHtml='';
for(const l of labels){labelHtml+='<span class="tag tag-'+escapeHtml(l.name)+'">'+escapeHtml(l.name)+'</span>';}
let editBtn='';
if(currentUser&&post.user&&currentUser.login===post.user.login){
editBtn='<button class="btn btn-outline btn-sm" onclick="openEditModal('+number+')">编辑</button>';
}
let html='<div class="post-detail">';
html+='<h1>'+escapeHtml(post.title)+'</h1>';
html+='<div class="meta">'+(avatar?'<img class="user-avatar" src="'+escapeHtml(avatar)+'">':'')+' <span>'+escapeHtml(user)+'</span> · <span>'+date+'</span> '+labelHtml+'</div>';
html+='<div class="body">'+renderMarkdown(post.body||'')+'</div>';
html+='<div class="actions">';
html+='<button class="btn btn-outline btn-sm" onclick="addReaction('+number+',\'heart\')">❤️ '+hearts+'</button>';
html+='<button class="btn btn-outline btn-sm" onclick="addReaction('+number+',\'+1\')">👍 '+likes+'</button>';
html+=editBtn;
html+='<a class="btn btn-outline btn-sm" href="'+escapeHtml(post.html_url)+'" target="_blank">在GitHub查看</a>';
html+='</div>';
html+='</div>';
html+='<div class="section-title"><h2>评论 ('+(post.comments||0)+')</h2></div>';
html+='<div id="comments-list"><div class="loading">加载评论...</div></div>';
if(userToken){
html+='<div class="comment-form">';
html+='<div class="form-group"><label>发表评论（支持Markdown）</label>';
html+='<textarea id="comment-input" placeholder="写下你的评论..."></textarea></div>';
html+='<button class="btn btn-primary btn-sm" onclick="submitComment('+number+')">发送</button>';
html+='</div>';
}else{
html+='<div class="comment-form"><p style="color:var(--text-dim)">请先<a class="btn-login" onclick="openLoginModal()">登录</a>后发表评论</p></div>';
}
content.innerHTML=html;
loadComments(number);
}).catch(e=>{content.innerHTML='<div class="loading">网络错误</div>';});
}
function loadComments(number){
fetch(API_BASE+'/api/comments?number='+number).then(r=>r.json()).then(data=>{
const list=document.getElementById('comments-list');
if(!list)return;
if(!data||data.length===0){list.innerHTML='<div class="empty"><p>暂无评论，快来发表第一条吧！</p></div>';return;}
let html='';
for(const c of data){
const user=c.user?c.user.login:'unknown';
const avatar=c.user?c.user.avatar_url:'';
const date=timeAgo(c.created_at);
html+='<div class="comment"><div class="meta">'+(avatar?'<img src="'+escapeHtml(avatar)+'">':'')+' <strong>'+escapeHtml(user)+'</strong> · '+date+'</div><div class="body">'+renderMarkdown(c.body||'')+'</div></div>';
}
list.innerHTML=html;
}).catch(()=>{const list=document.getElementById('comments-list');if(list)list.innerHTML='<div class="loading">加载评论失败</div>';});
}
function submitComment(number){
if(!userToken){showToast('请先登录',true);return;}
const body=document.getElementById('comment-input').value.trim();
if(!body){showToast('评论内容不能为空',true);return;}
fetch(API_BASE+'/api/comment',{
method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({token:userToken,number,body})
}).then(r=>r.json()).then(data=>{
if(data.error){showToast(data.error,true);return;}
showToast('评论成功！');
document.getElementById('comment-input').value='';
loadComments(number);
// 更新详情页评论数
showDetail(number);
}).catch(()=>showToast('网络错误',true));
}
function addReaction(number,content){
if(!userToken){showToast('请先登录',true);return;}
fetch(API_BASE+'/api/reaction',{
method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({token:userToken,number,content})
}).then(r=>r.json()).then(data=>{
if(data.error){showToast(data.error,true);return;}
showToast('操作成功！');
showDetail(number);
}).catch(()=>showToast('网络错误',true));
}

// === 发帖 ===
function openPostModal(type){
if(!userToken){showToast('请先登录后发帖',true);openLoginModal();return;}
currentModalType=type;
const titles={discussion:'发起讨论',bug:'提交漏洞报告',library:'提交第三方库'};
document.getElementById('post-modal-title').textContent=titles[type]||'发帖';
document.getElementById('lib-url-group').style.display=(type==='library')?'block':'none';
document.getElementById('post-title').value='';
document.getElementById('post-body').value='';
document.getElementById('lib-url').value='';
document.getElementById('post-modal').classList.add('active');
}
function closePostModal(){document.getElementById('post-modal').classList.remove('active');}
document.getElementById('post-modal').addEventListener('click',e=>{if(e.target.id==='post-modal')closePostModal();});
document.getElementById('submit-post').addEventListener('click',()=>{
if(!userToken){showToast('请先登录',true);return;}
const title=document.getElementById('post-title').value.trim();
const body=document.getElementById('post-body').value.trim();
if(!title){showToast('请输入标题',true);return;}
if(!body){showToast('请输入内容',true);return;}
let fullBody=body;
if(currentModalType==='library'){
const url=document.getElementById('lib-url').value.trim();
if(url)fullBody+='\n\n**库链接**: '+url;
}
const btn=document.getElementById('submit-post');
btn.textContent='提交中...';btn.disabled=true;
fetch(API_BASE+'/api/issues',{
method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({token:userToken,title,body:fullBody,labels:[currentModalType]})
}).then(r=>r.json()).then(data=>{
btn.textContent='提交';btn.disabled=false;
if(data.error){showToast(data.error,true);return;}
showToast('发布成功！');
closePostModal();
if(currentModalType==='discussion')loadPosts('forum-posts','discussion');
if(currentModalType==='bug')loadPosts('bug-posts','bug');
if(currentModalType==='library')loadPosts('lib-posts','library');
}).catch(e=>{btn.textContent='提交';btn.disabled=false;showToast('网络错误',true);});
});

// === 编辑帖子 ===
function openEditModal(number){
if(!userToken){showToast('请先登录',true);return;}
fetch(API_BASE+'/api/issue?number='+number).then(r=>r.json()).then(post=>{
if(post.error){showToast(post.error,true);return;}
if(!currentUser||!post.user||currentUser.login!==post.user.login){showToast('只能编辑自己的帖子',true);return;}
document.getElementById('edit-title').value=post.title||'';
document.getElementById('edit-body').value=post.body||'';
document.getElementById('edit-modal').classList.add('active');
document.getElementById('submit-edit').onclick=()=>{
const title=document.getElementById('edit-title').value.trim();
const body=document.getElementById('edit-body').value.trim();
if(!title||!body){showToast('标题和内容不能为空',true);return;}
const btn=document.getElementById('submit-edit');
btn.textContent='保存中...';btn.disabled=true;
fetch(API_BASE+'/api/issues/edit',{
method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({token:userToken,number,title,body})
}).then(r=>r.json()).then(data=>{
btn.textContent='保存';btn.disabled=false;
if(data.error){showToast(data.error,true);return;}
showToast('编辑成功！');
closeEditModal();
showDetail(number);
}).catch(()=>{btn.textContent='保存';btn.disabled=false;showToast('网络错误',true);});
};
}).catch(()=>showToast('网络错误',true));
}
function closeEditModal(){document.getElementById('edit-modal').classList.remove('active');}
document.getElementById('edit-modal').addEventListener('click',e=>{if(e.target.id==='edit-modal')closeEditModal();});

// === 搜索 ===
function doSearch(){
const q=document.getElementById('search-input').value.trim();
if(!q)return;
document.querySelectorAll('section').forEach(s=>s.classList.remove('active'));
document.getElementById('search').classList.add('active');
const results=document.getElementById('search-results');
results.innerHTML='<div class="loading">搜索中...</div>';
fetch(API_BASE+'/api/search?q='+encodeURIComponent(q)).then(r=>r.json()).then(data=>{
if(data.error){results.innerHTML='<div class="loading">'+escapeHtml(data.error)+'</div>';return;}
const items=data.items||[];
if(items.length===0){results.innerHTML='<div class="empty"><p>未找到相关帖子</p></div>';return;}
let html='';
for(const p of items){
const date=timeAgo(p.created_at);
const user=p.user?p.user.login:'unknown';
const avatar=p.user?p.user.avatar_url:'';
const comments=p.comments||0;
const labels=p.labels||[];
let labelHtml='';
for(const l of labels){labelHtml+='<span class="tag tag-'+escapeHtml(l.name)+'">'+escapeHtml(l.name)+'</span>';}
const num=p.number||0;
html+=`<div class="card" onclick="showDetail(${num})">
<h3>${escapeHtml(p.title)}</h3>
<div class="meta">${avatar?'<img class="user-avatar" style="width:20px;height:20px" src="'+escapeHtml(avatar)+'">':''} <span>${escapeHtml(user)}</span> · <span>${date}</span> ${labelHtml}</div>
<div class="body">${escapeHtml(p.body?p.body.substring(0,200):'')}</div>
<div class="stats"><span>💬 ${comments}</span></div>
</div>`;
}
results.innerHTML=html;
}).catch(()=>{results.innerHTML='<div class="loading">搜索失败</div>';});
}


// === 初始化 ===
updateUserUI();
routeTo('home');
</script>
</body>
</html>)HTML";
