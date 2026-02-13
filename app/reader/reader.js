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
    
    // 切换调试模式
    toggleDebug: function() {
        YUI.log("Toggling debug mode...");
        
        if (typeof YUI.inspect === 'undefined') {
            YUI.log("YUI.inspect is not available");
            return;
        }
        
        try {
            // 切换 inspect 模式
            if (this.debugMode) {
                YUI.inspect.disable();
                YUI.inspect.setShowBounds(false);
                YUI.inspect.setShowInfo(false);
                YUI.setText("debugButton", "Debug");
                YUI.log("Debug mode disabled");
            } else {
                YUI.inspect.enable();
                YUI.inspect.setShowBounds(true);
                YUI.inspect.setShowInfo(true);
                YUI.setText("debugButton", "Debug*");
                YUI.log("Debug mode enabled");
            }
            
            this.debugMode = !this.debugMode;
        } catch (e) {
            YUI.log("Error toggling debug mode: " + e.message);
        }
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
    
    // 将内容分割成页面（支持换行）
    splitContentIntoPages: function(content) {
        const maxCharsPerLine = 42;  // 每行最大字符数（根据字体大小和宽度计算）
        const maxLinesPerPage = 18;  // 每页最大行数（留出边距）
        const pages = [];
        
        // 按原始换行符分割段落
        const paragraphs = content.split('\n');
        let currentPageLines = [];
        let lineCount = 0;
        
        for (let p = 0; p < paragraphs.length; p++) {
            const paragraph = paragraphs[p];
            
            // 处理空段落（保留空行）
            if (!paragraph.trim()) {
                if (lineCount >= maxLinesPerPage) {
                    pages.push(currentPageLines.join('\n'));
                    currentPageLines = [];
                    lineCount = 0;
                }
                currentPageLines.push('');
                lineCount++;
                continue;
            }
            
            // 处理长段落，按句子分割
            let remainingText = paragraph;
            
            while (remainingText.length > 0) {
                // 如果已经接近页面末尾，尝试在句子边界分割
                if (lineCount >= maxLinesPerPage - 2) {
                    // 查找句子边界
                    let sentenceEnd = -1;
                    for (let i = 0; i < remainingText.length && i < 100; i++) {
                        const char = remainingText[i];
                        if (char === '。' || char === '！' || char === '？' || 
                            char === '.' || char === '!' || char === '?') {
                            sentenceEnd = i + 1;
                            break;
                        }
                    }
                    
                    if (sentenceEnd > 0) {
                        const sentence = remainingText.substring(0, sentenceEnd).trim();
                        remainingText = remainingText.substring(sentenceEnd).trim();
                        
                        // 分割长句子到多行
                        this.splitTextIntoLines(sentence, currentPageLines, maxCharsPerLine);
                        lineCount = currentPageLines.length % maxLinesPerPage;
                    } else {
                        // 找不到句子边界，强制分割
                        const chunk = remainingText.substring(0, Math.min(remainingText.length, maxCharsPerLine));
                        remainingText = remainingText.substring(chunk.length);
                        currentPageLines.push(chunk.trim());
                        lineCount++;
                    }
                } else {
                    // 页面有足够空间，正常处理
                    const chunk = remainingText.substring(0, Math.min(remainingText.length, maxCharsPerLine * 2));
                    remainingText = remainingText.substring(chunk.length);
                    
                    this.splitTextIntoLines(chunk, currentPageLines, maxCharsPerLine);
                    lineCount = currentPageLines.length % maxLinesPerPage;
                }
                
                // 检查是否需要新页面
                if (lineCount >= maxLinesPerPage) {
                    pages.push(currentPageLines.join('\n'));
                    currentPageLines = [];
                    lineCount = 0;
                }
            }
        }
        
        // 添加最后一页
        if (currentPageLines.length > 0) {
            pages.push(currentPageLines.join('\n'));
        }
        
        return pages;
    },
    
    // 将文本分割成多行
    splitTextIntoLines: function(text, linesArray, maxCharsPerLine) {
        let remaining = text.trim();
        
        while (remaining.length > 0) {
            if (remaining.length <= maxCharsPerLine) {
                // 剩余文本可以放入一行
                linesArray.push(remaining);
                break;
            }
            
            // 查找最佳分割点（优先在标点符号后，其次在空格）
            let splitPos = maxCharsPerLine;
            let bestSplitPos = -1;
            
            // 从右向左查找合适的分割点
            for (let i = maxCharsPerLine; i > maxCharsPerLine * 0.6; i--) {
                const char = remaining[i];
                if (char === ' ' || char === '　' || char === '，' || char === ',') {
                    bestSplitPos = i;
                    break;
                }
            }
            
            if (bestSplitPos === -1) {
                // 在标点符号后分割
                for (let i = maxCharsPerLine; i > maxCharsPerLine * 0.6; i--) {
                    const char = remaining[i];
                    if (char === '。' || char === '！' || char === '？' || 
                        char === '.' || char === '!' || char === '?') {
                        bestSplitPos = i + 1;
                        break;
                    }
                }
            }
            
            if (bestSplitPos === -1) {
                // 找不到合适的分割点，强制分割
                bestSplitPos = maxCharsPerLine;
            }
            
            const line = remaining.substring(0, bestSplitPos).trim();
            remaining = remaining.substring(bestSplitPos).trim();
            
            linesArray.push(line);
        }
    },
    
    // 显示指定页的内容
    displayPage: function(pageNumber) {
        YUI.log("displayPage called with pageNumber: " + pageNumber + ", totalPages: " + this.totalPages);
        
        if (pageNumber < 1 || pageNumber > this.totalPages) {
            YUI.log("Invalid page number: " + pageNumber);
            return;
        }
        
        // 更新当前页
        this.currentPage = pageNumber;
        
        // 获取章节标题（从第一页提取）
        const chapterTitle = this.extractChapterTitle(this.pages[0]);
        const pageContent = this.pages[pageNumber - 1];
        
        YUI.log("Chapter title: " + chapterTitle);
        YUI.log("Page content length: " + (pageContent ? pageContent.length : 0));
        
        // 更新UI
        this.updateContent(chapterTitle, pageContent);
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
        YUI.log("updateContent called with title: " + title + ", content length: " + (content ? content.length : 0));
        
        // 使用YUI的API更新UI
        try {
            YUI.setText("chapterTitle", title);
            YUI.log("chapterTitle updated successfully");
        } catch (e) {
            YUI.log("Error updating chapterTitle: " + e.message);
        }
        
        try {
            YUI.setText("contentText", content || "");
            YUI.log("contentText updated successfully");
        } catch (e) {
            YUI.log("Error updating contentText: " + e.message);
        }
        
        try {
            YUI.setText("bookSelector", this.currentBook);
            YUI.log("bookSelector updated successfully");
        } catch (e) {
            YUI.log("Error updating bookSelector: " + e.message);
        }
        
        YUI.log("Content updated: " + (title ? title.substring(0, 20) : "null") + "...");
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
        
        // 更新主题切换按钮文本
        const themeText = newTheme === "light" ? "主题" : "主题*";
        YUI.setText("themeToggle", themeText);
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
    },
    
    // 调试模式状态
    debugMode: false
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
    global.toggleDebug = readerApp.toggleDebug.bind(readerApp);
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
    window.toggleDebug = readerApp.toggleDebug.bind(readerApp);
}

initialize()