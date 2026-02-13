// 阅读器应用逻辑
const readerApp = {
    // 初始化应用
    init: function() {
        YUI.log("Reader app initializing...");
        
        // 设置初始状态
        this.currentBook = "book1.txt";
        this.currentPage = 1;
        this.fontSize = 16;
        this.theme = "light";
        this.totalPages = 0;
        this.pages = [];
        this.bookProgress = {};
        
        // 应用初始主题
        this.applyTheme("light");
        
        // 加载初始书籍
        this.loadBook("book1.txt", 1);
        
        YUI.log("Reader app initialized");
    },
    
    // 加载书籍内容
    loadBook: function(bookId, page) {
        YUI.log("Loading book: " + bookId + ", page: " + page);
        
        // 保存当前书籍的阅读进度
        this.saveReadingProgress();
        
        // 更新状态
        this.currentBook = bookId;
        this.currentPage = page || 1;
        
        // 从文件加载书籍内容
        this.loadBookFromFile(bookId, page);
    },
    
    // 从文件加载书籍内容
    loadBookFromFile: function(bookId, page) {
        const bookPath = "app/reader/books/" + bookId;
        
        YUI.log("Loading book from path: " + bookPath);
        
        // 确保 YUI.readFile 已正确绑定
        if (typeof YUI.readFile !== "function") {
            YUI.log("YUI.readFile is not a function. Please check the binding.");
        }

        // 使用YUI的文件读取API读取文件内容
        // try {
            const content = YUI.readFile(bookPath);
            
            if (content) {
                YUI.log("Content loaded successfully: " + content.substring(0, 50) + "...");

                // 将内容按页分割
                this.pages = this.splitContentIntoPages(content);
                this.totalPages = this.pages.length;

                YUI.log("Book split into " + this.totalPages + " pages");
                
                // 显示指定页的内容
                this.displayPage(Math.min(page || 1, this.totalPages));
            } else {
                YUI.log("Failed to load content from file: " + bookPath);
                this.displayError("无法加载书籍: " + bookId);
            }
        // } catch (e) {
        //     YUI.log("Error loading book content: " + e.message);
        //     this.displayError("加载书籍时出错: " + e.message);
        // }
    },
    
    // 将内容分割成页面
    splitContentIntoPages: function(content) {
        const maxCharsPerPage = 200;
        const pages = [];
        let startIndex = 0;
        
        while (startIndex < content.length) {
            // 计算当前页的结束位置
            let endIndex = startIndex + maxCharsPerPage;
            
            if (endIndex >= content.length) {
                // 如果是最后一页，直接添加剩余内容
                pages.push(content.substring(startIndex));
                break;
            }
            
            // 尝试在句号、感叹号或问号处分割
            let splitIndex = -1;
            for (let i = endIndex; i > startIndex + 200; i--) {
                const char = content[i];
                if (char === '。' || char === '！' || char === '？' || char === '.' || char === '!' || char === '?') {
                    splitIndex = i + 1;
                    break;
                }
            }
            
            // 如果找不到合适的分割点，就在空格处分割
            if (splitIndex === -1) {
                for (let i = endIndex; i > startIndex + 200; i--) {
                    if (content[i] === ' ' || content[i] === '\n') {
                        splitIndex = i;
                        break;
                    }
                }
            }
            
            // 如果还是找不到合适的分割点，就强制分割
            if (splitIndex === -1) {
                splitIndex = endIndex;
            }
            
            // 添加页面
            pages.push(content.substring(startIndex, splitIndex).trim());
            startIndex = splitIndex;
        }
        
        return pages;
    },
    
    // 显示指定页的内容
    displayPage: function(pageNumber) {
        if (pageNumber < 1 || pageNumber > this.totalPages) return;
        
        // 更新当前页
        this.currentPage = pageNumber;
        
        // 获取章节标题（从第一页提取）
        const chapterTitle = this.extractChapterTitle(this.pages[0]);
        
        // 更新UI
        this.updateContent(chapterTitle, this.pages[pageNumber - 1]);
        this.updatePageInfo(pageNumber, this.totalPages);
        this.updateProgress(pageNumber, this.totalPages);
    },
    
    // 提取章节标题
    extractChapterTitle: function(content) {
        // 尝试从内容中提取第一行作为标题
        const lines = content.split('\n');
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i].trim();
            if (line && (line.includes("第") || line.includes("章"))) {
                return line;
            }
        }
        
        // 如果没有找到明显的标题，返回默认标题
        return "未命名章节";
    },
    
    // 更新内容显示
    updateContent: function(title, content) {
        // 使用YUI的API更新UI
        YUI.setText("chapterTitle", title);
        YUI.setText("contentText", content);
        YUI.setText("bookSelector", this.currentBook);
        
        YUI.log("Content updated: " + title.substring(0, 20) + "...");
    },
    
    // 更新页面信息
    updatePageInfo: function(currentPage, totalPages) {
        const pageInfoText = `${currentPage} / ${totalPages}`;
        const pageIndicatorText = `第 ${currentPage} 页，共 ${totalPages} 页`;
        
        // 使用YUI的API更新UI
        YUI.setText("pageNumber", pageInfoText);
        YUI.setText("pageIndicator", pageIndicatorText);
    },
    
    // 更新进度
    updateProgress: function(currentPage, totalPages) {
        const percentage = Math.round((currentPage / totalPages) * 100);
        
        // 更新进度条
        YUI.setProperty("progressBar", "value", percentage);
        
        // 更新阅读进度
        if (!this.bookProgress) {
            this.bookProgress = {};
        }
        
        this.bookProgress[this.currentBook] = {
            page: currentPage,
            progress: percentage
        };
    },
    
    // 保存阅读进度
    saveReadingProgress: function() {
        YUI.log("Saving reading progress for: " + this.currentBook + ", page: " + this.currentPage);
        // 在实际应用中，这里应该保存到本地存储
    },
    
    // 切换主题
    toggleTheme: function() {
        const currentTheme = this.theme || "light";
        const newTheme = currentTheme === "light" ? "dark" : "light";
        
        this.applyTheme(newTheme);
        this.theme = newTheme;
        
        // 更新主题切换按钮图标
        const themeIcon = newTheme === "light" ? "🌙" : "☀️";
        YUI.setText("themeToggle", themeIcon);
    },
    
    // 应用主题
    applyTheme: function(themeName) {
        YUI.log("Applying theme: " + themeName);
        
        // 使用YUI的主题API
        const themePath = "app/reader/reader-theme-" + themeName + ".json";
        YUI.themeLoad(themePath);
        YUI.themeSetCurrent(themeName);
        YUI.themeApplyToTree();
        
        YUI.log("Theme applied: " + themeName);
    },
    
    // 上一页
    goToPrevPage: function() {
        if (this.currentPage > 1) {
            this.displayPage(this.currentPage - 1);
        }
    },
    
    // 下一页
    goToNextPage: function() {
        if (this.currentPage < this.totalPages) {
            this.displayPage(this.currentPage + 1);
        }
    },
    
    // 更改字体大小
    changeFontSize: function(delta) {
        const currentSize = this.fontSize || 16;
        let newSize = currentSize + delta;
        
        // 限制字体大小在12px到24px之间
        newSize = Math.max(12, Math.min(24, newSize));
        
        if (newSize !== currentSize) {
            this.fontSize = newSize;
            
            // 更新内容文本的字体大小
            YUI.setProperty("contentText", "fontSize", newSize + "px");
            
            // 更新字体大小标签
            YUI.setText("fontSizeLabel", newSize + "px");
            
            YUI.log("Font size changed to: " + newSize);
        }
    },
    
    // 减小字体
    decreaseFont: function() {
        this.changeFontSize(-2);
    },
    
    // 增大字体
    increaseFont: function() {
        this.changeFontSize(2);
    },
    
    // 显示错误信息
    displayError: function(message) {
        YUI.setText("contentText", `错误: ${message}`);
    }
};

// 初始化应用
function initialize() {
    YUI.log("Initializing reader app...");
    readerApp.init();
    YUI.log("Reader app initialized successfully");
}

// 导出函数供YUI调用
if (typeof global !== 'undefined') {
    global.initialize = initialize;
    global.loadBook = readerApp.loadBook.bind(readerApp);
    global.goToPrevPage = readerApp.goToPrevPage.bind(readerApp);
    global.goToNextPage = readerApp.goToNextPage.bind(readerApp);
    global.toggleTheme = readerApp.toggleTheme.bind(readerApp);
    global.decreaseFont = readerApp.decreaseFont.bind(readerApp);
    global.increaseFont = readerApp.increaseFont.bind(readerApp);
}

// 如果是浏览器环境，也添加到window对象
if (typeof window !== 'undefined') {
    window.initialize = initialize;
    window.loadBook = readerApp.loadBook.bind(readerApp);
    window.goToPrevPage = readerApp.goToPrevPage.bind(readerApp);
    window.goToNextPage = readerApp.goToNextPage.bind(readerApp);
    window.toggleTheme = readerApp.toggleTheme.bind(readerApp);
    window.decreaseFont = readerApp.decreaseFont.bind(readerApp);
    window.increaseFont = readerApp.increaseFont.bind(readerApp);
}

initialize()