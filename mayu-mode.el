;;; mayu-mode.el --- mayu setting editing commands for Emacs

;;; Copyright (C) 2000 Free Software Foundation, Inc.

;; Author: TAGA Nayuta <nayuta@ganaware.org>
;; Maintainer: TAGA Nayuta <nayuta@ganaware.org>
;; Keywords: languages, faces

(defvar
  mayu-font-lock-keywords
  `((,(concat
       "\\<\\("
       "[AMCWS]-"
       "\\|IC-"
       ;;"\\|I-"
       "\\|[INCS]L-"
       "\\|M[0-9]-"
       "\\|L[0-9]-"
       "\\|U-"
       "\\|D-"
       "\\|E[01]-"
       "\\)"
       ) . font-lock-keyword-face)
    ("#.*$" . font-lock-comment-face)
    ("/[^/\n]*/" . font-lock-string-face)
    ("\\\\$" . font-lock-warning-face)
    (,(concat
       "^\\s *\\<\\("
       "key"
       "\\|event\\s +\\(prefixed\\|after-key-up\\|before-key-down\\)"
       "\\|keyseq"
       "\\|def\\s +\\(key\\|alias\\|mod\\|sync\\)"
       "\\|mod"
       "\\|keymap"
       "\\|keymap2"
       "\\|window"
       "\\|include"
       "\\|if"
       "\\|define"
       "\\|else"
       "\\|elseif"
       "\\|elsif"
       "\\|endif"
       "\\)\\>"
       ) . font-lock-builtin-face)
    (,(concat
       "&\\("
       "Default"
       "\\|KeymapParent"
       "\\|KeymapWindow"
       "\\|OtherWindowClass"
       "\\|Prefix"
       "\\|Keymap"
       "\\|Sync"
       "\\|Toggle"
       "\\|EditNextModifier"
       "\\|Variable"
       "\\|Repeat"
       "\\|Undefined"
       "\\|Ignore"
       "\\|PostMessage"
       "\\|ShellExecute"
       "\\|LoadSetting"
       "\\|VK"
       "\\|Wait"
       "\\|InvestigateCommand"
       "\\|MayuDialog"
       "\\|DescribeBindings"
       "\\|HelpMessage"
       "\\|HelpVariable"
       "\\|WindowRaise"
       "\\|WindowLower"
       "\\|WindowMinimize"
       "\\|WindowMaximize"
       "\\|WindowHMaximize"
       "\\|WindowVMaximize"
       "\\|WindowMove"
       "\\|WindowMoveVisibly"
       "\\|WindowMoveTo"
       "\\|WindowClingToLeft"
       "\\|WindowClingToRight"
       "\\|WindowClingToTop"
       "\\|WindowClingToBottom"
       "\\|WindowClose"
       "\\|WindowToggleTopMost"
       "\\|WindowIdentify"
       "\\|WindowSetAlpha"
       "\\|WindowRedraw"
       "\\|WindowResizeTo"
       "\\|MouseMove"
       "\\|MouseWheel"
       "\\|ClipboardUpcaseWord"
       "\\|ClipboardDowncaseWord"
       "\\|ClipboardCopy"
       "\\|EmacsEditKillLinePred"
       "\\|EmacsEditKillLineFunc"
       "\\)\\>"
       ) . font-lock-builtin-face)
    "Default font-lock-keywords for mayu mode."))

(defvar mayu-mode-syntax-table nil
  "syntax table used in mayu mode")
(setq mayu-mode-syntax-table (make-syntax-table))
(modify-syntax-entry ?# "<\n" mayu-mode-syntax-table)
(modify-syntax-entry ?\n ">#" mayu-mode-syntax-table)

(defvar mayu-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "\C-c\C-c" 'comment-region)
    map))

;;;###autoload
(defun mayu-mode ()
  "A major mode to edit mayu setting files."
  (interactive)
  (kill-all-local-variables)
  (use-local-map mayu-mode-map)

  (make-local-variable 'comment-start)
  (setq comment-start "#")
  (make-local-variable 'parse-sexp-ignore-comments)
  (setq parse-sexp-ignore-comments t)

  (make-local-variable	'font-lock-defaults)
  (setq major-mode 'mayu-mode
	mode-name "mayu"
	font-lock-defaults '(mayu-font-lock-keywords nil)
	)
  (set-syntax-table mayu-mode-syntax-table)
  (run-hooks 'mayu-mode-hook))

(provide 'mayu-mode)

;;; mayu-mode.el ends here
