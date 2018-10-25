#include "qwt_mml_document.h"
#include "qwt_mml_entity_table.h"

#include <qapplication.h>
#include <qdebug.h>
#include <qdesktopwidget.h>
#include <qdom.h>
#include <qfontdatabase.h>
#include <qmap.h>
#include <qmath.h>
#include <qpainter.h>

// *******************************************************************
// Declarations
// *******************************************************************

static const qreal   g_mfrac_spacing          = 0.05;
static const qreal   g_mroot_base_margin      = 0.1;
static const qreal   g_mroot_base_line        = 0.5;
static const qreal   g_script_size_multiplier = 0.5;
static const char *  g_subsup_spacing         = "veryverythinmathspace";
static const qreal   g_min_font_point_size    = 8.0;
static const ushort  g_radical                = ( 0x22 << 8 ) | 0x1B;
static const int     g_oper_spec_rows         = 9;

static const int g_radical_points_size = 11;
static const QPointF g_radical_points[] = { QPointF( 0.0,         0.344439758 ),
                                            QPointF( 0.217181096, 0.419051636 ),
                                            QPointF( 0.557377049, 0.102829829 ),
                                            QPointF( 0.942686988, 1.048864253 ),
                                            QPointF( 1.0,         1.048864253 ),
                                            QPointF( 1.0,         1.0         ),
                                            QPointF( 1.0,         1.0         ),
                                            QPointF( 0.594230277, 0.0         ),
                                            QPointF( 0.516457480, 0.0         ),
                                            QPointF( 0.135213883, 0.352172079 ),
                                            QPointF( 0.024654201, 0.316221808 )
                                          };

static QwtMMLEntityTable mmlEntityTable;

struct QwtMml
{
    enum NodeType
    {
        NoNode = 0, MiNode, MnNode, MfracNode, MrowNode, MsqrtNode,
        MrootNode, MsupNode, MsubNode, MsubsupNode, MoNode,
        MstyleNode, TextNode, MphantomNode, MfencedNode,
        MtableNode, MtrNode, MtdNode, MoverNode, MunderNode,
        MunderoverNode, MerrorNode, MtextNode, MpaddedNode,
        MspaceNode, MalignMarkNode, UnknownNode
    };

    enum MathVariant
    {
        NormalMV       = 0x0000,
        BoldMV         = 0x0001,
        ItalicMV       = 0x0002,
        DoubleStruckMV = 0x0004,
        ScriptMV       = 0x0008,
        FrakturMV      = 0x0010,
        SansSerifMV    = 0x0020,
        MonospaceMV    = 0x0040
    };

    enum FormType { PrefixForm, InfixForm, PostfixForm };
    enum ColAlign { ColAlignLeft, ColAlignCenter, ColAlignRight };
    enum RowAlign { RowAlignTop, RowAlignCenter, RowAlignBottom,
                    RowAlignAxis, RowAlignBaseline
                  };
    enum FrameType { FrameNone, FrameSolid, FrameDashed };

    struct FrameSpacing
    {
        FrameSpacing( qreal hor = 0.0, qreal ver = 0.0 )
            : m_hor( hor ), m_ver( ver ) {}
        qreal m_hor, m_ver;
    };
};

struct QwtMmlOperSpec
{
    enum StretchDir { NoStretch, HStretch, VStretch, HVStretch };

#if 1
    QString name;
#endif
    QwtMml::FormType form;
    const char *attributes[g_oper_spec_rows];
    StretchDir stretch_dir;
};

struct QwtMmlNodeSpec
{
    QwtMml::NodeType type;
    const char *tag;
    const char *type_str;
    int child_spec;
    const char *child_types;
    const char *attributes;

    enum ChildSpec
    {
        ChildAny     = -1, // any number of children allowed
        ChildIgnore  = -2, // do not build subexpression of children
        ImplicitMrow = -3  // if more than one child, build mrow
    };
};

typedef QMap<QString, QString> QwtMmlAttributeMap;
class QwtMmlNode;

class QwtMmlDocument : public QwtMml
{
public:
    QwtMmlDocument();
    ~QwtMmlDocument();
    void clear();

    bool setContent( const QString &text, QString *errorMsg = 0,
                     int *errorLine = 0, int *errorColumn = 0 );
    void paint( QPainter *painter, const QPointF &pos );
    void dump() const;
    QSizeF size() const;
    void layout();

    QString fontName( QwtMathMLDocument::MmlFont type ) const;
    void setFontName( QwtMathMLDocument::MmlFont type, const QString &name );

    qreal baseFontPointSize() const { return m_base_font_point_size; }
    void setBaseFontPointSize( qreal size ) { m_base_font_point_size = size; }

    QColor foregroundColor() const { return m_foreground_color; }
    void setForegroundColor( const QColor &color ) { m_foreground_color = color; }

    QColor backgroundColor() const { return m_background_color; }
    void setBackgroundColor( const QColor &color ) { m_background_color = color; }

#ifdef MML_TEST
    bool drawFrames() const { return m_draw_frames; }
    void setDrawFrames( const bool &drawFrames ) { m_draw_frames = drawFrames; }
#endif

private:
    void _dump( const QwtMmlNode *node, const QString &indent ) const;
    bool insertChild( QwtMmlNode *parent, QwtMmlNode *new_node,
                      QString *errorMsg );

    QwtMmlNode *domToMml( const QDomNode &dom_node, bool *ok,
                          QString *errorMsg );
    QwtMmlNode *createNode( NodeType type,
                            const QwtMmlAttributeMap &mml_attr,
                            const QString &mml_value, QString *errorMsg );
    QwtMmlNode *createImplicitMrowNode( const QDomNode &dom_node, bool *ok,
                                     QString *errorMsg );

    void insertOperator( QwtMmlNode *node, const QString &text );

    QwtMmlNode *m_root_node;

    QString m_normal_font_name;
    QString m_fraktur_font_name;
    QString m_sans_serif_font_name;
    QString m_script_font_name;
    QString m_monospace_font_name;
    QString m_doublestruck_font_name;
    qreal m_base_font_point_size;
    QColor m_foreground_color;
    QColor m_background_color;
#ifdef MML_TEST
    bool m_draw_frames;
#endif
};

class QwtMmlNode : public QwtMml
{
    friend class QwtMmlDocument;

public:
    QwtMmlNode( NodeType type, QwtMmlDocument *document,
                const QwtMmlAttributeMap &attribute_map );
    virtual ~QwtMmlNode();

    // Mml stuff
    NodeType nodeType() const { return m_node_type; }

    virtual QString toStr() const;

    void setRelOrigin( const QPointF &rel_origin );

    void stretchTo( const QRectF &rect );
    QPointF devicePoint( const QPointF &p ) const;

    QRectF myRect() const { return m_my_rect; }
    virtual void setMyRect( const QRectF &rect ) { m_my_rect = rect; }
    void updateMyRect();

    QRectF parentRect() const;
    virtual QRectF deviceRect() const;

    virtual void stretch();
    virtual void layout();
    virtual void paint( QPainter *painter, qreal x_scaling, qreal y_scaling );

    qreal basePos() const;

    qreal em() const;
    qreal ex() const;

    QString explicitAttribute( const QString &name, const QString &def = QString() ) const;
    QString inheritAttributeFromMrow( const QString &name, const QString &def = QString() ) const;

    virtual QFont font() const;
    virtual QColor color() const;
    virtual QColor background() const;
    virtual int scriptlevel( const QwtMmlNode *child = 0 ) const;


    // Node stuff
    QwtMmlNode *parent() const { return m_parent; }
    QwtMmlNode *firstChild() const { return m_first_child; }
    QwtMmlNode *nextSibling() const { return m_next_sibling; }
    QwtMmlNode *previousSibling() const { return m_previous_sibling; }
    QwtMmlNode *lastSibling() const;
    QwtMmlNode *firstSibling() const;
    bool isLastSibling() const { return m_next_sibling == 0; }
    bool isFirstSibling() const { return m_previous_sibling == 0; }
    bool hasChildNodes() const { return m_first_child != 0; }

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal, qreal ) const;
    virtual QRectF symbolRect() const { return QRectF( 0.0, 0.0, 0.0, 0.0 ); }

    QwtMmlNode *parentWithExplicitAttribute( const QString &name, NodeType type = NoNode );
    qreal interpretSpacing( const QString &value, bool *ok ) const;

    qreal lineWidth() const;

private:
    QwtMmlAttributeMap m_attribute_map;
    bool m_stretched;
    QRectF m_my_rect, m_parent_rect;
    QPointF m_rel_origin;

    NodeType m_node_type;
    QwtMmlDocument *m_document;

    QwtMmlNode *m_parent,
               *m_first_child,
               *m_next_sibling,
               *m_previous_sibling;
};

class QwtMmlTokenNode : public QwtMmlNode
{
public:
    QwtMmlTokenNode( NodeType type, QwtMmlDocument *document,
                  const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( type, document, attribute_map ) {}

    QString text() const;
};

class QwtMmlMphantomNode : public QwtMmlNode
{
public:
    QwtMmlMphantomNode( QwtMmlDocument *document,
                     const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MphantomNode, document, attribute_map ) {}

    virtual void paint( QPainter *, qreal, qreal ) {}
};

class QwtMmlUnknownNode : public QwtMmlNode
{
public:
    QwtMmlUnknownNode( QwtMmlDocument *document,
                    const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( UnknownNode, document, attribute_map ) {}
};

class QwtMmlMfencedNode : public QwtMmlNode
{
public:
    QwtMmlMfencedNode( QwtMmlDocument *document,
                    const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MfencedNode, document, attribute_map ) {}
};

class QwtMmlMalignMarkNode : public QwtMmlNode
{
public:
    QwtMmlMalignMarkNode( QwtMmlDocument *document )
        : QwtMmlNode( MalignMarkNode, document, QwtMmlAttributeMap() ) {}
};

class QwtMmlMfracNode : public QwtMmlNode
{
public:
    QwtMmlMfracNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MfracNode, document, attribute_map ) {}

    QwtMmlNode *numerator() const;
    QwtMmlNode *denominator() const;

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

private:
    qreal lineThickness() const;
};

class QwtMmlMrowNode : public QwtMmlNode
{
public:
    QwtMmlMrowNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MrowNode, document, attribute_map ) {}
};

class QwtMmlRootBaseNode : public QwtMmlNode
{
public:
    QwtMmlRootBaseNode( NodeType type, QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( type, document, attribute_map ) {}

    QwtMmlNode *base() const;
    QwtMmlNode *index() const;

    virtual int scriptlevel( const QwtMmlNode *child = 0 ) const;

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

private:
    QRectF baseRect() const;
    QRectF radicalRect() const;
    qreal radicalMargin() const;
    qreal radicalLineWidth() const;
};

class QwtMmlMrootNode : public QwtMmlRootBaseNode
{
public:
    QwtMmlMrootNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlRootBaseNode( MrootNode, document, attribute_map ) {}
};

class QwtMmlMsqrtNode : public QwtMmlRootBaseNode
{
public:
    QwtMmlMsqrtNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlRootBaseNode( MsqrtNode, document, attribute_map ) {}

};


class QwtMmlTextNode : public QwtMmlNode
{
public:
    QwtMmlTextNode( const QString &text, QwtMmlDocument *document );

    virtual QString toStr() const;
    QString text() const { return m_text; }

    // TextNodes are not xml elements, so they can't have attributes of
    // their own. Everything is taken from the parent.
    virtual QFont font() const { return parent()->font(); }
    virtual int scriptlevel( const QwtMmlNode* = 0 ) const { return parent()->scriptlevel( this ); }
    virtual QColor color() const { return parent()->color(); }
    virtual QColor background() const { return parent()->background(); }

protected:
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

    QString m_text;

private:
    bool isInvisibleOperator() const;
};

class QwtMmlMiNode : public QwtMmlTokenNode
{
public:
    QwtMmlMiNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlTokenNode( MiNode, document, attribute_map ) {}
};

class QwtMmlMnNode : public QwtMmlTokenNode
{
public:
    QwtMmlMnNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlTokenNode( MnNode, document, attribute_map ) {}
};

class QwtMmlSubsupBaseNode : public QwtMmlNode
{
public:
    QwtMmlSubsupBaseNode( NodeType type, QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( type, document, attribute_map ) {}

    QwtMmlNode *base() const;
    QwtMmlNode *sscript() const;

    virtual int scriptlevel( const QwtMmlNode *child = 0 ) const;
};

class QwtMmlMsupNode : public QwtMmlSubsupBaseNode
{
public:
    QwtMmlMsupNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlSubsupBaseNode( MsupNode, document, attribute_map ) {}

protected:
    virtual void layoutSymbol();
};

class QwtMmlMsubNode : public QwtMmlSubsupBaseNode
{
public:
    QwtMmlMsubNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlSubsupBaseNode( MsubNode, document, attribute_map ) {}

protected:
    virtual void layoutSymbol();
};

class QwtMmlMsubsupNode : public QwtMmlNode
{
public:
    QwtMmlMsubsupNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MsubsupNode, document, attribute_map ) {}

    QwtMmlNode *base() const;
    QwtMmlNode *superscript() const;
    QwtMmlNode *subscript() const;

    virtual int scriptlevel( const QwtMmlNode *child = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class QwtMmlMoNode : public QwtMmlTokenNode
{
public:
    QwtMmlMoNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map );

    QString dictionaryAttribute( const QString &name ) const;
    virtual void stretch();
    virtual qreal lspace() const;
    virtual qreal rspace() const;

    virtual QString toStr() const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;

    virtual FormType form() const;

private:
    const QwtMmlOperSpec *m_oper_spec;
    bool unaryMinus() const;
};

class QwtMmlMstyleNode : public QwtMmlNode
{
public:
    QwtMmlMstyleNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MstyleNode, document, attribute_map ) {}
};

class QwtMmlTableBaseNode : public QwtMmlNode
{
public:
    QwtMmlTableBaseNode( NodeType type, QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( type, document, attribute_map ) {}
};

class QwtMmlMtableNode : public QwtMmlTableBaseNode
{
public:
    QwtMmlMtableNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlTableBaseNode( MtableNode, document, attribute_map ) {}

    qreal rowspacing() const;
    qreal columnspacing() const;
    qreal framespacing_hor() const;
    qreal framespacing_ver() const;
    FrameType frame() const;
    FrameType columnlines( int idx ) const;
    FrameType rowlines( int idx ) const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;

private:
    struct CellSizeData
    {
        void init( const QwtMmlNode *first_row );
        QList<qreal> col_widths, row_heights;
        int numCols() const { return col_widths.count(); }
        int numRows() const { return row_heights.count(); }
        qreal colWidthSum() const;
        qreal rowHeightSum() const;
    };

    CellSizeData m_cell_size_data;
    qreal m_content_width, m_content_height;
};

class QwtMmlMtrNode : public QwtMmlTableBaseNode
{
public:
    QwtMmlMtrNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlTableBaseNode( MtrNode, document, attribute_map ) {}
    void layoutCells( const QList<qreal> &col_widths, qreal col_spc );
};

class QwtMmlMtdNode : public QwtMmlTableBaseNode
{
public:
    QwtMmlMtdNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlTableBaseNode( MtdNode, document, attribute_map ) { m_scriptlevel_adjust = 0; }
    virtual void setMyRect( const QRectF &rect );

    ColAlign columnalign();
    RowAlign rowalign();
    int colNum() const;
    int rowNum() const;
    virtual int scriptlevel( const QwtMmlNode *child = 0 ) const;

private:
    int m_scriptlevel_adjust; // added or subtracted to scriptlevel to
                              // make contents fit the cell
};

class QwtMmlMoverNode : public QwtMmlNode
{
public:
    QwtMmlMoverNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MoverNode, document, attribute_map ) {}
    virtual int scriptlevel( const QwtMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class QwtMmlMunderNode : public QwtMmlNode
{
public:
    QwtMmlMunderNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MunderNode, document, attribute_map ) {}
    virtual int scriptlevel( const QwtMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class QwtMmlMunderoverNode : public QwtMmlNode
{
public:
    QwtMmlMunderoverNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MunderoverNode, document, attribute_map ) {}
    virtual int scriptlevel( const QwtMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class QwtMmlMerrorNode : public QwtMmlNode
{
public:
    QwtMmlMerrorNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MerrorNode, document, attribute_map ) {}
};

class QwtMmlMtextNode : public QwtMmlNode
{
public:
    QwtMmlMtextNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( MtextNode, document, attribute_map ) {}
};

class QwtMmlSpacingNode : public QwtMmlNode
{
public:
    QwtMmlSpacingNode( const NodeType &node_type, QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlNode( node_type, document, attribute_map ) {}

public:
    qreal width() const;
    qreal height() const;
    qreal depth() const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;

    qreal interpretSpacing( QString value, qreal base_value, bool *ok ) const;
};

class QwtMmlMpaddedNode : public QwtMmlSpacingNode
{
public:
    QwtMmlMpaddedNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlSpacingNode( MpaddedNode, document, attribute_map ) {}

public:
    virtual qreal lspace() const;

protected:
    virtual QRectF symbolRect() const;
};

class QwtMmlMspaceNode : public QwtMmlSpacingNode
{
public:
    QwtMmlMspaceNode( QwtMmlDocument *document, const QwtMmlAttributeMap &attribute_map )
        : QwtMmlSpacingNode( MspaceNode, document, attribute_map ) {}
};

static const QwtMmlNodeSpec *mmlFindNodeSpec( QwtMml::NodeType type );
static const QwtMmlNodeSpec *mmlFindNodeSpec( const QString &tag );
static bool mmlCheckChildType( QwtMml::NodeType parent_type,
                               QwtMml::NodeType child_type, QString *error_str );
static bool mmlCheckAttributes( QwtMml::NodeType child_type,
                                const QwtMmlAttributeMap &attr, QString *error_str );
static QString mmlDictAttribute( const QString &name, const QwtMmlOperSpec *spec );
static const QwtMmlOperSpec *mmlFindOperSpec( const QString &name, QwtMml::FormType form );
static qreal mmlInterpretSpacing( QString name, qreal em, qreal ex, bool *ok );
static qreal mmlInterpretPercentSpacing( QString value, qreal base, bool *ok );
static int mmlInterpretMathVariant( const QString &value, bool *ok );
static QwtMml::FormType mmlInterpretForm( const QString &value, bool *ok );
static QwtMml::FrameType mmlInterpretFrameType( const QString &value_list, int idx, bool *ok );
static QwtMml::FrameSpacing mmlInterpretFrameSpacing( const QString &value_list, qreal em, qreal ex, bool *ok );
static QwtMml::ColAlign mmlInterpretColAlign( const QString &value_list, int colnum, bool *ok );
static QwtMml::RowAlign mmlInterpretRowAlign( const QString &value_list, int rownum, bool *ok );
static QwtMml::FrameType mmlInterpretFrameType( const QString &value_list, int idx, bool *ok );
static QFont mmlInterpretDepreciatedFontAttr( const QwtMmlAttributeMap &font_attr, QFont &fn, qreal em, qreal ex );
static QFont mmlInterpretMathSize( const QString &value, QFont &fn, qreal em, qreal ex, bool *ok );
static QString mmlInterpretListAttr( const QString &value_list, int idx, const QString &def );
static qreal mmlQstringToQreal( const QString &string, bool *ok = 0 );

#define MML_ATT_COMMON      " class style id xref actiontype "
#define MML_ATT_FONTSIZE    " fontsize fontweight fontstyle fontfamily color "
#define MML_ATT_MATHVARIANT " mathvariant mathsize mathcolor mathbackground "
#define MML_ATT_FONTINFO    MML_ATT_FONTSIZE MML_ATT_MATHVARIANT
#define MML_ATT_OPINFO      " form fence separator lspace rspace stretchy symmetric " \
    " maxsize minsize largeop movablelimits accent "
#define MML_ATT_SIZEINFO    " width height depth "
#define MML_ATT_TABLEINFO   " align rowalign columnalign columnwidth groupalign " \
    " alignmentscope side rowspacing columnspacing rowlines " \
    " columnlines width frame framespacing equalrows " \
    " equalcolumns displaystyle "
#define MML_ATT_MFRAC       " bevelled numalign denomalign linethickness "
#define MML_ATT_MSTYLE      MML_ATT_FONTINFO MML_ATT_OPINFO \
    " scriptlevel lquote rquote linethickness displaystyle " \
    " scriptsizemultiplier scriptminsize background " \
    " veryverythinmathspace verythinmathspace thinmathspace " \
    " mediummathspace thickmathspace verythickmathspace " \
    " veryverythickmathspace open close separators " \
    " subscriptshift superscriptshift accentunder tableinfo " \
    " rowspan columnspan edge selection bevelled "
#define MML_ATT_MTABLE      " align rowalign columnalign groupalign alignmentscope " \
    " columnwidth width rowspacing columnspacing rowlines columnlines " \
    " frame framespacing equalrows equalcolumns displaystyle side " \
    " minlabelspacing "

static const QwtMmlNodeSpec g_node_spec_data[] =
{
//    type                    tag           type_str          child_spec                    child_types              attributes ""=none, 0=any
//    ----------------------- ------------- ----------------- ----------------------------- ------------------------ ---------------------------------------------------------------------
    { QwtMml::MiNode,         "mi",         "MiNode",         QwtMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO                                       },
    { QwtMml::MnNode,         "mn",         "MnNode",         QwtMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO                                       },
    { QwtMml::MfracNode,      "mfrac",      "MfracNode",      2,                            0,                       MML_ATT_COMMON MML_ATT_MFRAC                                          },
    { QwtMml::MrowNode,       "mrow",       "MrowNode",       QwtMmlNodeSpec::ChildAny,     0,                       MML_ATT_COMMON " display mode "                                       },
    { QwtMml::MsqrtNode,      "msqrt",      "MsqrtNode",      QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { QwtMml::MrootNode,      "mroot",      "MrootNode",      2,                            0,                       MML_ATT_COMMON                                                        },
    { QwtMml::MsupNode,       "msup",       "MsupNode",       2,                            0,                       MML_ATT_COMMON " subscriptshift "                                     },
    { QwtMml::MsubNode,       "msub",       "MsubNode",       2,                            0,                       MML_ATT_COMMON " superscriptshift "                                   },
    { QwtMml::MsubsupNode,    "msubsup",    "MsubsupNode",    3,                            0,                       MML_ATT_COMMON " subscriptshift superscriptshift "                    },
    { QwtMml::MoNode,         "mo",         "MoNode",         QwtMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO MML_ATT_OPINFO                        },
    { QwtMml::MstyleNode,     "mstyle",     "MstyleNode",     QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON MML_ATT_MSTYLE                                         },
    { QwtMml::MphantomNode,   "mphantom",   "MphantomNode",   QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { QwtMml::MalignMarkNode, "malignmark", "MalignMarkNode", 0,                            0,                       ""                                                                    },
    { QwtMml::MfencedNode,    "mfenced",    "MfencedNode",    QwtMmlNodeSpec::ChildAny,     0,                       MML_ATT_COMMON " open close separators "                              },
    { QwtMml::MtableNode,     "mtable",     "MtableNode",     QwtMmlNodeSpec::ChildAny,     " MtrNode ",             MML_ATT_COMMON MML_ATT_MTABLE                                         },
    { QwtMml::MtrNode,        "mtr",        "MtrNode",        QwtMmlNodeSpec::ChildAny,     " MtdNode ",             MML_ATT_COMMON " rowalign columnalign groupalign "                    },
    { QwtMml::MtdNode,        "mtd",        "MtdNode",        QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " rowspan columnspan rowalign columnalign groupalign " },
    { QwtMml::MoverNode,      "mover",      "MoverNode",      2,                            0,                       MML_ATT_COMMON " accent align "                                       },
    { QwtMml::MunderNode,     "munder",     "MunderNode",     2,                            0,                       MML_ATT_COMMON " accentunder align "                                  },
    { QwtMml::MunderoverNode, "munderover", "MunderoverNode", 3,                            0,                       MML_ATT_COMMON " accentunder accent align "                           },
    { QwtMml::MerrorNode,     "merror",     "MerrorNode",     QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { QwtMml::MtextNode,      "mtext",      "MtextNode",      1,                            " TextNode ",            MML_ATT_COMMON " width height depth linebreak "                       },
    { QwtMml::MpaddedNode,    "mpadded",    "MpaddedNode",    QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " width height depth lspace "                          },
    { QwtMml::MspaceNode,     "mspace",     "MspaceNode",     QwtMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " width height depth linebreak "                       },
    { QwtMml::TextNode,       0,            "TextNode",       QwtMmlNodeSpec::ChildIgnore,  0,                       ""                                                                    },
    { QwtMml::UnknownNode,    0,            "UnknownNode",    QwtMmlNodeSpec::ChildAny,     0,                       0                                                                     },
    { QwtMml::NoNode,         0,            0,                0,                            0,                       0                                                                     }
};

static const char *g_oper_spec_names[g_oper_spec_rows] =
{
    "accent", "fence", "largeop", "lspace", "minsize", "movablelimits",
    "rspace", "separator", "stretchy" /* stretchdir */
};

static const QwtMmlOperSpec g_oper_spec_data[] =
{
//                                                                accent   fence    largeop  lspace               minsize movablelimits rspace                   separator stretchy
//                                                                -------- -------- -------- -------------------- ------- ------------- ------------------------ --------- --------
    { "!",                                 QwtMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "!"
    { "!!",                                QwtMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "!!"
    { "!=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "!="
    { "&And;",                             QwtMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&And;"
    { "&ApplyFunction;",                   QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&ApplyFunction;"
    { "&Assign;",                          QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Assign;"
    { "&Backslash;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&Backslash;"
    { "&Because;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Because;"
    { "&Breve;",                           QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Breve;"
    { "&Cap;",                             QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Cap;"
    { "&CapitalDifferentialD;",            QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CapitalDifferentialD;"
    { "&Cedilla;",                         QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Cedilla;"
    { "&CenterDot;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CenterDot;"
    { "&CircleDot;",                       QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CircleDot;"
    { "&CircleMinus;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CircleMinus;"
    { "&CirclePlus;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CirclePlus;"
    { "&CircleTimes;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CircleTimes;"
    { "&ClockwiseContourIntegral;",        QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&ClockwiseContourIntegral;"
    { "&CloseCurlyDoubleQuote;",           QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CloseCurlyDoubleQuote;"
    { "&CloseCurlyQuote;",                 QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CloseCurlyQuote;"
    { "&Colon;",                           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Colon;"
    { "&Congruent;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Congruent;"
    { "&ContourIntegral;",                 QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&ContourIntegral;"
    { "&Coproduct;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Coproduct;"
    { "&CounterClockwiseContourIntegral;", QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&CounterClockwiseContourInteg
    { "&Cross;",                           QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Cross;"
    { "&Cup;",                             QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Cup;"
    { "&CupCap;",                          QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&CupCap;"
    { "&Del;",                             QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Del;"
    { "&DiacriticalAcute;",                QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DiacriticalAcute;"
    { "&DiacriticalDot;",                  QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DiacriticalDot;"
    { "&DiacriticalDoubleAcute;",          QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DiacriticalDoubleAcute;"
    { "&DiacriticalGrave;",                QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DiacriticalGrave;"
    { "&DiacriticalLeftArrow;",            QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalLeftArrow;"
    { "&DiacriticalLeftRightArrow;",       QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalLeftRightArrow;"
    { "&DiacriticalLeftRightVector;",      QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalLeftRightVector;"
    { "&DiacriticalLeftVector;",           QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalLeftVector;"
    { "&DiacriticalRightArrow;",           QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalRightArrow;"
    { "&DiacriticalRightVector;",          QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DiacriticalRightVector;"
    { "&DiacriticalTilde;",                QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::NoStretch }, // "&DiacriticalTilde;"
    { "&Diamond;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Diamond;"
    { "&DifferentialD;",                   QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DifferentialD;"
    { "&DotEqual;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DotEqual;"
    { "&DoubleContourIntegral;",           QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DoubleContourIntegral;"
    { "&DoubleDot;",                       QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DoubleDot;"
    { "&DoubleDownArrow;",                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DoubleDownArrow;"
    { "&DoubleLeftArrow;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleLeftArrow;"
    { "&DoubleLeftRightArrow;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleLeftRightArrow;"
    { "&DoubleLeftTee;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DoubleLeftTee;"
    { "&DoubleLongLeftArrow;",             QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleLongLeftArrow;"
    { "&DoubleLongLeftRightArrow;",        QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleLongLeftRightArrow;"
    { "&DoubleLongRightArrow;",            QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleLongRightArrow;"
    { "&DoubleRightArrow;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&DoubleRightArrow;"
    { "&DoubleRightTee;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DoubleRightTee;"
    { "&DoubleUpArrow;",                   QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DoubleUpArrow;"
    { "&DoubleUpDownArrow;",               QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DoubleUpDownArrow;"
    { "&DoubleVerticalBar;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DoubleVerticalBar;"
    { "&DownArrow;",                       QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DownArrow;"
    { "&DownArrowBar;",                    QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DownArrowBar;"
    { "&DownArrowUpArrow;",                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DownArrowUpArrow;"
    { "&DownBreve;",                       QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DownBreve;"
    { "&DownLeftRightVector;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownLeftRightVector;"
    { "&DownLeftTeeVector;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownLeftTeeVector;"
    { "&DownLeftVector;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownLeftVector;"
    { "&DownLeftVectorBar;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownLeftVectorBar;"
    { "&DownRightTeeVector;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownRightTeeVector;"
    { "&DownRightVector;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownRightVector;"
    { "&DownRightVectorBar;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&DownRightVectorBar;"
    { "&DownTee;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&DownTee;"
    { "&DownTeeArrow;",                    QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&DownTeeArrow;"
    { "&Element;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Element;"
    { "&Equal;",                           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Equal;"
    { "&EqualTilde;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&EqualTilde;"
    { "&Equilibrium;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&Equilibrium;"
    { "&Exists;",                          QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Exists;"
    { "&ForAll;",                          QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&ForAll;"
    { "&GreaterEqual;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterEqual;"
    { "&GreaterEqualLess;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterEqualLess;"
    { "&GreaterFullEqual;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterFullEqual;"
    { "&GreaterGreater;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterGreater;"
    { "&GreaterLess;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterLess;"
    { "&GreaterSlantEqual;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterSlantEqual;"
    { "&GreaterTilde;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&GreaterTilde;"
    { "&Hacek;",                           QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::NoStretch }, // "&Hacek;"
    { "&Hat;",                             QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&Hat;"
    { "&HorizontalLine;",                  QwtMml::InfixForm,   { 0,       0,       0,       "0em",               "0",    0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&HorizontalLine;"
    { "&HumpDownHump;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&HumpDownHump;"
    { "&HumpEqual;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&HumpEqual;"
    { "&Implies;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&Implies;"
    { "&Integral;",                        QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Integral;"
    { "&Intersection;",                    QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&Intersection;"
    { "&InvisibleComma;",                  QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   "true",   0        }, QwtMmlOperSpec::NoStretch }, // "&InvisibleComma;"
    { "&InvisibleTimes;",                  QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&InvisibleTimes;"
    { "&LeftAngleBracket;",                QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&LeftAngleBracket;"
    { "&LeftArrow;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftArrow;"
    { "&LeftArrowBar;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftArrowBar;"
    { "&LeftArrowRightArrow;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftArrowRightArrow;"
    { "&LeftBracketingBar;",               QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&LeftBracketingBar;"
    { "&LeftCeiling;",                     QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&LeftCeiling;"
    { "&LeftDoubleBracket;",               QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftDoubleBracket;"
    { "&LeftDoubleBracketingBar;",         QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftDoubleBracketingBar;"
    { "&LeftDownTeeVector;",               QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftDownTeeVector;"
    { "&LeftDownVector;",                  QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftDownVector;"
    { "&LeftDownVectorBar;",               QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftDownVectorBar;"
    { "&LeftFloor;",                       QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&LeftFloor;"
    { "&LeftRightArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftRightArrow;"
    { "&LeftRightVector;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftRightVector;"
    { "&LeftSkeleton;",                    QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LeftSkeleton;"
    { "&LeftTee;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LeftTee;"
    { "&LeftTeeArrow;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftTeeArrow;"
    { "&LeftTeeVector;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftTeeVector;"
    { "&LeftTriangle;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LeftTriangle;"
    { "&LeftTriangleBar;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LeftTriangleBar;"
    { "&LeftTriangleEqual;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LeftTriangleEqual;"
    { "&LeftUpDownVector;",                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftUpDownVector;"
    { "&LeftUpTeeVector;",                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftUpTeeVector;"
    { "&LeftUpVector;",                    QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftUpVector;"
    { "&LeftUpVectorBar;",                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LeftUpVectorBar;"
    { "&LeftVector;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LeftVector;"
    { "&LeftVectorBar;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&LeftVectorBar;"
    { "&LessEqualGreater;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessEqualGreater;"
    { "&LessFullEqual;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessFullEqual;"
    { "&LessGreater;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessGreater;"
    { "&LessLess;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessLess;"
    { "&LessSlantEqual;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessSlantEqual;"
    { "&LessTilde;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&LessTilde;"
    { "&LongLeftArrow;",                   QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LongLeftArrow;"
    { "&LongLeftRightArrow;",              QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LongLeftRightArrow;"
    { "&LongRightArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&LongRightArrow;"
    { "&LowerLeftArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LowerLeftArrow;"
    { "&LowerRightArrow;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&LowerRightArrow;"
    { "&MinusPlus;",                       QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, QwtMmlOperSpec::NoStretch }, // "&MinusPlus;"
    { "&NestedGreaterGreater;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NestedGreaterGreater;"
    { "&NestedLessLess;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NestedLessLess;"
    { "&Not;",                             QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Not;"
    { "&NotCongruent;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotCongruent;"
    { "&NotCupCap;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotCupCap;"
    { "&NotDoubleVerticalBar;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotDoubleVerticalBar;"
    { "&NotElement;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotElement;"
    { "&NotEqual;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotEqual;"
    { "&NotEqualTilde;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotEqualTilde;"
    { "&NotExists;",                       QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotExists;"
    { "&NotGreater;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreater;"
    { "&NotGreaterEqual;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterEqual;"
    { "&NotGreaterFullEqual;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterFullEqual;"
    { "&NotGreaterGreater;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterGreater;"
    { "&NotGreaterLess;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterLess;"
    { "&NotGreaterSlantEqual;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterSlantEqual;"
    { "&NotGreaterTilde;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotGreaterTilde;"
    { "&NotHumpDownHump;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotHumpDownHump;"
    { "&NotHumpEqual;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotHumpEqual;"
    { "&NotLeftTriangle;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLeftTriangle;"
    { "&NotLeftTriangleBar;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLeftTriangleBar;"
    { "&NotLeftTriangleEqual;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLeftTriangleEqual;"
    { "&NotLess;",                         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLess;"
    { "&NotLessEqual;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessEqual;"
    { "&NotLessFullEqual;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessFullEqual;"
    { "&NotLessGreater;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessGreater;"
    { "&NotLessLess;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessLess;"
    { "&NotLessSlantEqual;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessSlantEqual;"
    { "&NotLessTilde;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotLessTilde;"
    { "&NotNestedGreaterGreater;",         QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotNestedGreaterGreater;"
    { "&NotNestedLessLess;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotNestedLessLess;"
    { "&NotPrecedes;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotPrecedes;"
    { "&NotPrecedesEqual;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotPrecedesEqual;"
    { "&NotPrecedesSlantEqual;",           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotPrecedesSlantEqual;"
    { "&NotPrecedesTilde;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotPrecedesTilde;"
    { "&NotReverseElement;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotReverseElement;"
    { "&NotRightTriangle;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotRightTriangle;"
    { "&NotRightTriangleBar;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotRightTriangleBar;"
    { "&NotRightTriangleEqual;",           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotRightTriangleEqual;"
    { "&NotSquareSubset;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSquareSubset;"
    { "&NotSquareSubsetEqual;",            QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSquareSubsetEqual;"
    { "&NotSquareSuperset;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSquareSuperset;"
    { "&NotSquareSupersetEqual;",          QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSquareSupersetEqual;"
    { "&NotSubset;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSubset;"
    { "&NotSubsetEqual;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSubsetEqual;"
    { "&NotSucceeds;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSucceeds;"
    { "&NotSucceedsEqual;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSucceedsEqual;"
    { "&NotSucceedsSlantEqual;",           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSucceedsSlantEqual;"
    { "&NotSucceedsTilde;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSucceedsTilde;"
    { "&NotSuperset;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSuperset;"
    { "&NotSupersetEqual;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotSupersetEqual;"
    { "&NotTilde;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotTilde;"
    { "&NotTildeEqual;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotTildeEqual;"
    { "&NotTildeFullEqual;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotTildeFullEqual;"
    { "&NotTildeTilde;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotTildeTilde;"
    { "&NotVerticalBar;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&NotVerticalBar;"
    { "&OpenCurlyDoubleQuote;",            QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&OpenCurlyDoubleQuote;"
    { "&OpenCurlyQuote;",                  QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&OpenCurlyQuote;"
    { "&Or;",                              QwtMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&Or;"
    { "&OverBar;",                         QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&OverBar;"
    { "&OverBrace;",                       QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&OverBrace;"
    { "&OverBracket;",                     QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&OverBracket;"
    { "&OverParenthesis;",                 QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&OverParenthesis;"
    { "&PartialD;",                        QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&PartialD;"
    { "&PlusMinus;",                       QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, QwtMmlOperSpec::NoStretch }, // "&PlusMinus;"
    { "&Precedes;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Precedes;"
    { "&PrecedesEqual;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&PrecedesEqual;"
    { "&PrecedesSlantEqual;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&PrecedesSlantEqual;"
    { "&PrecedesTilde;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&PrecedesTilde;"
    { "&Product;",                         QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Product;"
    { "&Proportion;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Proportion;"
    { "&Proportional;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Proportional;"
    { "&ReverseElement;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&ReverseElement;"
    { "&ReverseEquilibrium;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&ReverseEquilibrium;"
    { "&ReverseUpEquilibrium;",            QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&ReverseUpEquilibrium;"
    { "&RightAngleBracket;",               QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightAngleBracket;"
    { "&RightArrow;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightArrow;"
    { "&RightArrowBar;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightArrowBar;"
    { "&RightArrowLeftArrow;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightArrowLeftArrow;"
    { "&RightBracketingBar;",              QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightBracketingBar;"
    { "&RightCeiling;",                    QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightCeiling;"
    { "&RightDoubleBracket;",              QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightDoubleBracket;"
    { "&RightDoubleBracketingBar;",        QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightDoubleBracketingBar;"
    { "&RightDownTeeVector;",              QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightDownTeeVector;"
    { "&RightDownVector;",                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightDownVector;"
    { "&RightDownVectorBar;",              QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightDownVectorBar;"
    { "&RightFloor;",                      QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightFloor;"
    { "&RightSkeleton;",                   QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RightSkeleton;"
    { "&RightTee;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RightTee;"
    { "&RightTeeArrow;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightTeeArrow;"
    { "&RightTeeVector;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightTeeVector;"
    { "&RightTriangle;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RightTriangle;"
    { "&RightTriangleBar;",                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RightTriangleBar;"
    { "&RightTriangleEqual;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RightTriangleEqual;"
    { "&RightUpDownVector;",               QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&RightUpDownVector;"
    { "&RightUpTeeVector;",                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightUpTeeVector;"
    { "&RightUpVector;",                   QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightUpVector;"
    { "&RightUpVectorBar;",                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&RightUpVectorBar;"
    { "&RightVector;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightVector;"
    { "&RightVectorBar;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&RightVectorBar;"
    { "&RoundImplies;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&RoundImplies;"
    { "&ShortDownArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&ShortDownArrow;"
    { "&ShortLeftArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::HStretch  }, // "&ShortLeftArrow;"
    { "&ShortRightArrow;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::HStretch  }, // "&ShortRightArrow;"
    { "&ShortUpArrow;",                    QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::VStretch  }, // "&ShortUpArrow;"
    { "&SmallCircle;",                     QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SmallCircle;"
    { "&Sqrt;",                            QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&Sqrt;"
    { "&Square;",                          QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Square;"
    { "&SquareIntersection;",              QwtMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&SquareIntersection;"
    { "&SquareSubset;",                    QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SquareSubset;"
    { "&SquareSubsetEqual;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SquareSubsetEqual;"
    { "&SquareSuperset;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SquareSuperset;"
    { "&SquareSupersetEqual;",             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SquareSupersetEqual;"
    { "&SquareUnion;",                     QwtMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&SquareUnion;"
    { "&Star;",                            QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Star;"
    { "&Subset;",                          QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Subset;"
    { "&SubsetEqual;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SubsetEqual;"
    { "&Succeeds;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Succeeds;"
    { "&SucceedsEqual;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SucceedsEqual;"
    { "&SucceedsSlantEqual;",              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SucceedsSlantEqual;"
    { "&SucceedsTilde;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SucceedsTilde;"
    { "&SuchThat;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SuchThat;"
    { "&Sum;",                             QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Sum;"
    { "&Superset;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Superset;"
    { "&SupersetEqual;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&SupersetEqual;"
    { "&Therefore;",                       QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Therefore;"
    { "&Tilde;",                           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Tilde;"
    { "&TildeEqual;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&TildeEqual;"
    { "&TildeFullEqual;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&TildeFullEqual;"
    { "&TildeTilde;",                      QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&TildeTilde;"
    { "&TripleDot;",                       QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "&TripleDot;"
    { "&UnderBar;",                        QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&UnderBar;"
    { "&UnderBrace;",                      QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&UnderBrace;"
    { "&UnderBracket;",                    QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&UnderBracket;"
    { "&UnderParenthesis;",                QwtMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::HStretch  }, // "&UnderParenthesis;"
    { "&Union;",                           QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&Union;"
    { "&UnionPlus;",                       QwtMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&UnionPlus;"
    { "&UpArrow;",                         QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpArrow;"
    { "&UpArrowBar;",                      QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpArrowBar;"
    { "&UpArrowDownArrow;",                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpArrowDownArrow;"
    { "&UpDownArrow;",                     QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpDownArrow;"
    { "&UpEquilibrium;",                   QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpEquilibrium;"
    { "&UpTee;",                           QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&UpTee;"
    { "&UpTeeArrow;",                      QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&UpTeeArrow;"
    { "&UpperLeftArrow;",                  QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&UpperLeftArrow;"
    { "&UpperRightArrow;",                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::HVStretch }, // "&UpperRightArrow;"
    { "&Vee;",                             QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Vee;"
    { "&VerticalBar;",                     QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&VerticalBar;"
    { "&VerticalLine;",                    QwtMml::InfixForm,   { 0,       0,       0,       "0em",               "0",    0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&VerticalLine;"
    { "&VerticalSeparator;",               QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "&VerticalSeparator;"
    { "&VerticalTilde;",                   QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&VerticalTilde;"
    { "&Wedge;",                           QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "&Wedge;"
    { "&amp;",                             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&amp;"
    { "&amp;&amp;",                        QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&amp;&amp;"
    { "&le;",                              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&le;"
    { "&lt;",                              QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&lt;"
    { "&lt;=",                             QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "&lt;="
    { "&lt;>",                             QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "&lt;>"
    { "'",                                 QwtMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "'"
    { "(",                                 QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "("
    { ")",                                 QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // ")"
    { "*",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "*"
    { "**",                                QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "**"
    { "*=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "*="
    { "+",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "+"
    { "+",                                 QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, QwtMmlOperSpec::NoStretch }, // "+"
    { "++",                                QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "++"
    { "+=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "+="
    { ",",                                 QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "verythickmathspace",    "true",   0        }, QwtMmlOperSpec::NoStretch }, // ","
    { "-",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "-"
    { "-",                                 QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, QwtMmlOperSpec::NoStretch }, // "-"
    { "--",                                QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "--"
    { "-=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "-="
    { "->",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "->"
    { ".",                                 QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "."
    { "..",                                QwtMml::PostfixForm, { 0,       0,       0,       "mediummathspace",   0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // ".."
    { "...",                               QwtMml::PostfixForm, { 0,       0,       0,       "mediummathspace",   0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // "..."
    { "/",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "/"
    { "//",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "//"
    { "/=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "/="
    { ":",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // ":"
    { ":=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // ":="
    { ";",                                 QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "verythickmathspace",    "true",   0        }, QwtMmlOperSpec::NoStretch }, // ";"
    { ";",                                 QwtMml::PostfixForm, { 0,       0,       0,       "0em",               0,      0,            "0em",                   "true",   0        }, QwtMmlOperSpec::NoStretch }, // ";"
    { "=",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "="
    { "==",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // "=="
    { ">",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // ">"
    { ">=",                                QwtMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, QwtMmlOperSpec::NoStretch }, // ">="
    { "?",                                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "?"
    { "@",                                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "@"
    { "[",                                 QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "["
    { "]",                                 QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "]"
    { "^",                                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "^"
    { "_",                                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "_"
    { "lim",                               QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "lim"
    { "max",                               QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "max"
    { "min",                               QwtMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, QwtMmlOperSpec::NoStretch }, // "min"
    { "{",                                 QwtMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "{"
    { "|",                                 QwtMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "|"
    { "||",                                QwtMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        0        }, QwtMmlOperSpec::NoStretch }, // "||"
    { "}",                                 QwtMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, QwtMmlOperSpec::VStretch  }, // "}"
    { "~",                                 QwtMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, QwtMmlOperSpec::NoStretch }, // "~"
#if 1
    { QString( QChar( 0x64, 0x20 ) ),      QwtMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, QwtMmlOperSpec::NoStretch }, // Invisible addition
#endif
    { 0,                                   QwtMml::InfixForm,   { 0,       0,       0,       0,                   0,      0,            0,                       0,        0        }, QwtMmlOperSpec::NoStretch }
};

static const QwtMmlOperSpec g_oper_spec_defaults =
{
    0,  QwtMml::InfixForm, { "false", "false", "false", "thickmathspace", "1", "false", "thickmathspace", "false",  "false" }, QwtMmlOperSpec::NoStretch
};

static const int g_oper_spec_count = sizeof( g_oper_spec_data ) / sizeof( QwtMmlOperSpec ) - 1;

// *******************************************************************
// QwtMmlDocument
// *******************************************************************

QString QwtMmlDocument::fontName( QwtMathMLDocument::MmlFont type ) const
{
    switch ( type )
    {
        case QwtMathMLDocument::NormalFont:
            return m_normal_font_name;
        case QwtMathMLDocument::FrakturFont:
            return m_fraktur_font_name;
        case QwtMathMLDocument::SansSerifFont:
            return m_sans_serif_font_name;
        case QwtMathMLDocument::ScriptFont:
            return m_script_font_name;
        case QwtMathMLDocument::MonospaceFont:
            return m_monospace_font_name;
        case QwtMathMLDocument::DoublestruckFont:
            return m_doublestruck_font_name;
    };

    return QString();
}

void QwtMmlDocument::setFontName( QwtMathMLDocument::MmlFont type,
                                  const QString &name )
{
    switch ( type )
    {
        case QwtMathMLDocument::NormalFont:
            m_normal_font_name = name;
            break;
        case QwtMathMLDocument::FrakturFont:
            m_fraktur_font_name = name;
            break;
        case QwtMathMLDocument::SansSerifFont:
            m_sans_serif_font_name = name;
            break;
        case QwtMathMLDocument::ScriptFont:
            m_script_font_name = name;
            break;
        case QwtMathMLDocument::MonospaceFont:
            m_monospace_font_name = name;
            break;
        case QwtMathMLDocument::DoublestruckFont:
            m_doublestruck_font_name = name;
            break;
    };
}

QwtMml::NodeType domToQwtMmlNodeType( const QDomNode &dom_node )
{
    QwtMml::NodeType mml_type = QwtMml::NoNode;

    switch ( dom_node.nodeType() )
    {
        case QDomNode::ElementNode:
        {
            QString tag = dom_node.nodeName();
            const QwtMmlNodeSpec *spec = mmlFindNodeSpec( tag );

            // treat urecognised tags as mrow
            if ( spec == 0 )
                mml_type = QwtMml::UnknownNode;
            else
                mml_type = spec->type;

            break;
        }
        case QDomNode::TextNode:
            mml_type = QwtMml::TextNode;
            break;

        case QDomNode::DocumentNode:
            mml_type = QwtMml::UnknownNode;
            break;

        case QDomNode::EntityReferenceNode:
#if 0
            qWarning() << "EntityReferenceNode: name=\"" + dom_node.nodeName() + "\" value=\"" + dom_node.nodeValue() + "\"";
#endif
            break;

        case QDomNode::AttributeNode:
        case QDomNode::CDATASectionNode:
        case QDomNode::EntityNode:
        case QDomNode::ProcessingInstructionNode:
        case QDomNode::CommentNode:
        case QDomNode::DocumentTypeNode:
        case QDomNode::DocumentFragmentNode:
        case QDomNode::NotationNode:
        case QDomNode::BaseNode:
        case QDomNode::CharacterDataNode:
            break;
    }

    return mml_type;
}


QwtMmlDocument::QwtMmlDocument()
{
    m_root_node = 0;

    // We set m_normal_font_name based on the information available at
    // https://vismor.com/documents/site_implementation/viewing_mathematics/S7.php
    // Note: on Linux, the Ubuntu, DejaVu Serif, FreeSerif and Liberation Serif
    //       either don't look that great or have rendering problems (e.g.
    //       FreeSerif doesn't render 0 properly!), so we simply use Century
    //       Schoolbook L...

    QFontDatabase font_database;

#if defined( Q_OS_WIN )
    if ( font_database.hasFamily( "Cambria" ) )
        m_normal_font_name = "Cambria";
    else if ( font_database.hasFamily( "Lucida Sans Unicode" ) )
        m_normal_font_name = "Lucida Sans Unicode";
    else
        m_normal_font_name = "Times New Roman";
#elif defined( Q_OS_LINUX )
    m_normal_font_name = "Century Schoolbook L";
#elif defined( Q_OS_MAC )
    if ( font_database.hasFamily( "STIXGeneral" ) )
        m_normal_font_name = "STIXGeneral";
    else
        m_normal_font_name = "Times New Roman";
#else
    m_normal_font_name = "Times New Roman";
#endif
    m_fraktur_font_name = "Fraktur";
    m_sans_serif_font_name = "Luxi Sans";
    m_script_font_name = "Urw Chancery L";
    m_monospace_font_name = "Luxi Mono";
    m_doublestruck_font_name = "Doublestruck";
    m_base_font_point_size = 16;
    m_foreground_color = Qt::black;
    m_background_color = Qt::white;

#ifdef MML_TEST
    m_draw_frames = false;
#endif
}

QwtMmlDocument::~QwtMmlDocument()
{
    clear();
}

void QwtMmlDocument::clear()
{
    delete m_root_node;
    m_root_node = 0;
}

void QwtMmlDocument::dump() const
{
    if ( m_root_node == 0 )
        return;

    QString indent;
    _dump( m_root_node, indent );
}

void QwtMmlDocument::_dump(
    const QwtMmlNode *node, const QString &indent ) const
{
    if ( node == 0 ) return;

    qWarning() << indent + node->toStr();

    const QwtMmlNode *child = node->firstChild();
    for ( ; child != 0; child = child->nextSibling() )
        _dump( child, indent + "  " );
}

bool QwtMmlDocument::setContent(
    const QString &text, QString *errorMsg, int *errorLine, int *errorColumn )
{
    clear();

    QDomDocument dom;
    if ( !dom.setContent( text, false, errorMsg, errorLine, errorColumn ) )
        return false;

    // we don't have access to line info from now on
    if ( errorLine != 0 ) *errorLine = -1;
    if ( errorColumn != 0 ) *errorColumn = -1;

    bool ok;
    QwtMmlNode *root_node = domToMml( dom, &ok, errorMsg );
    if ( !ok )
        return false;

    if ( root_node == 0 )
    {
        if ( errorMsg != 0 )
            *errorMsg = "empty document";
        return false;
    }

    insertChild( 0, root_node, 0 );
    layout();

    return true;
}

void QwtMmlDocument::layout()
{
    if ( m_root_node == 0 )
        return;

    m_root_node->layout();
    m_root_node->stretch();
}

bool QwtMmlDocument::insertChild( QwtMmlNode *parent, QwtMmlNode *new_node,
                                  QString *errorMsg )
{
    if ( new_node == 0 )
        return true;

    Q_ASSERT( new_node->parent() == 0
              && new_node->nextSibling() == 0
              && new_node->previousSibling() == 0 );

    if ( parent != 0 )
    {
        if ( !mmlCheckChildType( parent->nodeType(), new_node->nodeType(), errorMsg ) )
            return false;
    }

    if ( parent == 0 )
    {
        if ( m_root_node == 0 )
        {
            m_root_node = new_node;
        }
        else
        {
            QwtMmlNode *n = m_root_node->lastSibling();
            n->m_next_sibling = new_node;
            new_node->m_previous_sibling = n;
        }
    }
    else
    {
        new_node->m_parent = parent;
        if ( parent->hasChildNodes() )
        {
            QwtMmlNode *n = parent->firstChild()->lastSibling();
            n->m_next_sibling = new_node;
            new_node->m_previous_sibling = n;
        }
        else
        {
            parent->m_first_child = new_node;
        }
    }

    return true;
}

QwtMmlNode *QwtMmlDocument::createNode( NodeType type,
                                        const QwtMmlAttributeMap &mml_attr,
                                        const QString &mml_value,
                                        QString *errorMsg )
{
    Q_ASSERT( type != NoNode );

    QwtMmlNode *mml_node = 0;

    if ( !mmlCheckAttributes( type, mml_attr, errorMsg ) )
        return 0;

    switch ( type )
    {
        case MiNode:
            mml_node = new QwtMmlMiNode( this, mml_attr );
            break;
        case MnNode:
            mml_node = new QwtMmlMnNode( this, mml_attr );
            break;
        case MfracNode:
            mml_node = new QwtMmlMfracNode( this, mml_attr );
            break;
        case MrowNode:
            mml_node = new QwtMmlMrowNode( this, mml_attr );
            break;
        case MsqrtNode:
            mml_node = new QwtMmlMsqrtNode( this, mml_attr );
            break;
        case MrootNode:
            mml_node = new QwtMmlMrootNode( this, mml_attr );
            break;
        case MsupNode:
            mml_node = new QwtMmlMsupNode( this, mml_attr );
            break;
        case MsubNode:
            mml_node = new QwtMmlMsubNode( this, mml_attr );
            break;
        case MsubsupNode:
            mml_node = new QwtMmlMsubsupNode( this, mml_attr );
            break;
        case MoNode:
            mml_node = new QwtMmlMoNode( this, mml_attr );
            break;
        case MstyleNode:
            mml_node = new QwtMmlMstyleNode( this, mml_attr );
            break;
        case TextNode:
            mml_node = new QwtMmlTextNode( mml_value, this );
            break;
        case MphantomNode:
            mml_node = new QwtMmlMphantomNode( this, mml_attr );
            break;
        case MfencedNode:
            mml_node = new QwtMmlMfencedNode( this, mml_attr );
            break;
        case MtableNode:
            mml_node = new QwtMmlMtableNode( this, mml_attr );
            break;
        case MtrNode:
            mml_node = new QwtMmlMtrNode( this, mml_attr );
            break;
        case MtdNode:
            mml_node = new QwtMmlMtdNode( this, mml_attr );
            break;
        case MoverNode:
            mml_node = new QwtMmlMoverNode( this, mml_attr );
            break;
        case MunderNode:
            mml_node = new QwtMmlMunderNode( this, mml_attr );
            break;
        case MunderoverNode:
            mml_node = new QwtMmlMunderoverNode( this, mml_attr );
            break;
        case MalignMarkNode:
            mml_node = new QwtMmlMalignMarkNode( this );
            break;
        case MerrorNode:
            mml_node = new QwtMmlMerrorNode( this, mml_attr );
            break;
        case MtextNode:
            mml_node = new QwtMmlMtextNode( this, mml_attr );
            break;
        case MpaddedNode:
            mml_node = new QwtMmlMpaddedNode( this, mml_attr );
            break;
        case MspaceNode:
            mml_node = new QwtMmlMspaceNode( this, mml_attr );
            break;
        case UnknownNode:
            mml_node = new QwtMmlUnknownNode( this, mml_attr );
            break;
        case NoNode:
            mml_node = 0;
            break;
    }

    return mml_node;
}

void QwtMmlDocument::insertOperator( QwtMmlNode *node, const QString &text )
{
    QwtMmlNode *text_node = createNode( TextNode, QwtMmlAttributeMap(), text, 0 );
    QwtMmlNode *mo_node = createNode( MoNode, QwtMmlAttributeMap(), QString(), 0 );

    bool ok = insertChild( node, mo_node, 0 );
    Q_ASSERT( ok );
    ok = insertChild( mo_node, text_node, 0 );
    Q_ASSERT( ok );
    Q_UNUSED( ok );
}

QwtMmlNode *QwtMmlDocument::domToMml( const QDomNode &dom_node, bool *ok,
                                      QString *errorMsg )
{
    // create the node

    Q_ASSERT( ok != 0 );

    NodeType mml_type = domToQwtMmlNodeType( dom_node );

    if ( mml_type == NoNode )
    {
        *ok = true;
        return 0;
    }

    QDomNamedNodeMap dom_attr = dom_node.attributes();
    QwtMmlAttributeMap mml_attr;
    for ( int i = 0; i < dom_attr.length(); ++i )
    {
        QDomNode attr_node = dom_attr.item( i );
        Q_ASSERT( !attr_node.nodeName().isNull() );
        Q_ASSERT( !attr_node.nodeValue().isNull() );
        mml_attr[attr_node.nodeName()] = attr_node.nodeValue();
    }

    QString mml_value;
    if ( mml_type == TextNode )
        mml_value = dom_node.nodeValue();
    QwtMmlNode *mml_node = createNode( mml_type, mml_attr, mml_value, errorMsg );
    if ( mml_node == 0 )
    {
        *ok = false;
        return 0;
    }

    // create the node's children according to the child_spec

    const QwtMmlNodeSpec *spec = mmlFindNodeSpec( mml_type );
    QDomNodeList dom_child_list = dom_node.childNodes();
    int child_cnt = dom_child_list.count();
    QwtMmlNode *mml_child = 0;

    QString separator_list;
    if ( mml_type == MfencedNode )
        separator_list = mml_node->explicitAttribute( "separators", "," );

    switch ( spec->child_spec )
    {
        case QwtMmlNodeSpec::ChildIgnore:
            break;

        case QwtMmlNodeSpec::ImplicitMrow:

            if ( child_cnt > 0 )
            {
                mml_child = createImplicitMrowNode( dom_node, ok, errorMsg );
                if ( !*ok )
                {
                    delete mml_node;
                    return 0;
                }

                if ( !insertChild( mml_node, mml_child, errorMsg ) )
                {
                    delete mml_node;
                    delete mml_child;
                    *ok = false;
                    return 0;
                }
            }

            break;

        default:
            // exact ammount of children specified - check...
            if ( spec->child_spec != child_cnt )
            {
                if ( errorMsg != 0 )
                    *errorMsg = QString( "element " )
                                + spec->tag
                                + " requires exactly "
                                + QString::number( spec->child_spec )
                                + " arguments, got "
                                + QString::number( child_cnt );
                delete mml_node;
                *ok = false;
                return 0;
            }

            // ...and continue just as in ChildAny

        case QwtMmlNodeSpec::ChildAny:
            if ( mml_type == MfencedNode )
                insertOperator( mml_node, mml_node->explicitAttribute( "open", "(" ) );

            for ( int i = 0; i < child_cnt; ++i )
            {
                QDomNode dom_child = dom_child_list.item( i );

                QwtMmlNode *mml_child = domToMml( dom_child, ok, errorMsg );
                if ( !*ok )
                {
                    delete mml_node;
                    return 0;
                }

                if ( mml_type == MtableNode && mml_child->nodeType() != MtrNode )
                {
                    QwtMmlNode *mtr_node = createNode( MtrNode, QwtMmlAttributeMap(), QString(), 0 );
                    insertChild( mml_node, mtr_node, 0 );
                    if ( !insertChild( mtr_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }
                else if ( mml_type == MtrNode && mml_child->nodeType() != MtdNode )
                {
                    QwtMmlNode *mtd_node = createNode( MtdNode, QwtMmlAttributeMap(), QString(), 0 );
                    insertChild( mml_node, mtd_node, 0 );
                    if ( !insertChild( mtd_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }
                else
                {
                    if ( !insertChild( mml_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }

                if ( i < child_cnt - 1 && mml_type == MfencedNode && !separator_list.isEmpty() )
                {
                    QChar separator;
                    if ( i >= separator_list.length() )
                        separator = separator_list.at( separator_list.length() - 1 );
                    else
                        separator = separator_list[i];
                    insertOperator( mml_node, QString( separator ) );
                }
            }

            if ( mml_type == MfencedNode )
                insertOperator( mml_node, mml_node->explicitAttribute( "close", ")" ) );

            break;
    }

    *ok = true;
    return mml_node;
}

QwtMmlNode *QwtMmlDocument::createImplicitMrowNode( const QDomNode &dom_node,
                                                    bool *ok,
                                                    QString *errorMsg )
{
    QDomNodeList dom_child_list = dom_node.childNodes();
    int child_cnt = dom_child_list.count();

    if ( child_cnt == 0 )
    {
        *ok = true;
        return 0;
    }

    if ( child_cnt == 1 )
        return domToMml( dom_child_list.item( 0 ), ok, errorMsg );

    QwtMmlNode *mml_node = createNode( MrowNode, QwtMmlAttributeMap(),
                                       QString(), errorMsg );
    Q_ASSERT( mml_node != 0 ); // there is no reason in heaven or hell for this to fail

    for ( int i = 0; i < child_cnt; ++i )
    {
        QDomNode dom_child = dom_child_list.item( i );

        QwtMmlNode *mml_child = domToMml( dom_child, ok, errorMsg );
        if ( !*ok )
        {
            delete mml_node;
            return 0;
        }

        if ( !insertChild( mml_node, mml_child, errorMsg ) )
        {
            delete mml_node;
            delete mml_child;
            *ok = false;
            return 0;
        }
    }

    return mml_node;
}

void QwtMmlDocument::paint( QPainter *painter, const QPointF &pos )
{
    if ( m_root_node == 0 )
        return;

    m_root_node->setRelOrigin( pos - m_root_node->myRect().topLeft() );
    m_root_node->paint( painter, 1.0, 1.0 );
}

QSizeF QwtMmlDocument::size() const
{
    if ( m_root_node == 0 )
        return QSizeF( 0.0, 0.0 );
    return m_root_node->deviceRect().size();
}

// *******************************************************************
// QwtMmlNode
// *******************************************************************

QwtMmlNode::QwtMmlNode( NodeType type, QwtMmlDocument *document,
                        const QwtMmlAttributeMap &attribute_map )
{
    m_parent = 0;
    m_first_child = 0;
    m_next_sibling = 0;
    m_previous_sibling = 0;

    m_node_type = type;
    m_document = document;
    m_attribute_map = attribute_map;

    m_my_rect = m_parent_rect = QRectF( 0.0, 0.0, 0.0, 0.0 );
    m_rel_origin = QPointF( 0.0, 0.0 );
    m_stretched = false;
}

QwtMmlNode::~QwtMmlNode()
{
    QwtMmlNode *n = m_first_child;
    while ( n != 0 )
    {
        QwtMmlNode *tmp = n->nextSibling();
        delete n;
        n = tmp;
    }
}

static QString rectToStr( const QRectF &rect )
{
    return QString( "[(%1, %2), %3x%4]" )
           .arg( rect.x() )
           .arg( rect.y() )
           .arg( rect.width() )
           .arg( rect.height() );
}

QString QwtMmlNode::toStr() const
{
    const QwtMmlNodeSpec *spec = mmlFindNodeSpec( m_node_type );
    Q_ASSERT( spec != 0 );

    return QString( "%1 %2 mr=%3 pr=%4 dr=%5 ro=(%7, %8) str=%9" )
           .arg( spec->type_str )
           .arg( ( quintptr )this, 0, 16 )
           .arg( rectToStr( m_my_rect ) )
           .arg( rectToStr( parentRect() ) )
           .arg( rectToStr( deviceRect() ) )
           .arg( m_rel_origin.x() )
           .arg( m_rel_origin.y() )
           .arg( ( int )m_stretched );
}

qreal QwtMmlNode::interpretSpacing( const QString &value, bool *ok ) const
{
    return mmlInterpretSpacing( value, em(), ex(), ok );
}

qreal QwtMmlNode::lineWidth() const
{
    return qMax( 1.0, QFontMetricsF( font() ).lineWidth() );
}

qreal QwtMmlNode::basePos() const
{
    QFontMetricsF fm( font() );
    return fm.strikeOutPos();
}

QwtMmlNode *QwtMmlNode::lastSibling() const
{
    const QwtMmlNode *n = this;
    while ( !n->isLastSibling() )
        n = n->nextSibling();
    return const_cast<QwtMmlNode*>( n );
}

QwtMmlNode *QwtMmlNode::firstSibling() const
{
    const QwtMmlNode *n = this;
    while ( !n->isFirstSibling() )
        n = n->previousSibling();
    return const_cast<QwtMmlNode*>( n );
}

qreal QwtMmlNode::em() const
{
    return QFontMetricsF( font() ).boundingRect( 'm' ).width();
}

qreal QwtMmlNode::ex() const
{
    return QFontMetricsF( font() ).boundingRect( 'x' ).height();
}

int QwtMmlNode::scriptlevel( const QwtMmlNode * ) const
{
    int parent_sl;
    if ( m_parent == 0 )
        parent_sl = 0;
    else
        parent_sl = m_parent->scriptlevel( this );

    QString expl_sl_str = explicitAttribute( "scriptlevel" );
    if ( expl_sl_str.isNull() )
        return parent_sl;

    if ( expl_sl_str.startsWith( "+" ) || expl_sl_str.startsWith( "-" ) )
    {
        bool ok;
        int expl_sl = expl_sl_str.toInt( &ok );
        if ( ok )
        {
            return parent_sl + expl_sl;
        }
        else
        {
            qWarning() << "QwtMmlNode::scriptlevel(): bad value " + expl_sl_str;
            return parent_sl;
        }
    }

    bool ok;
    int expl_sl = expl_sl_str.toInt( &ok );
    if ( ok )
        return expl_sl;


    if ( expl_sl_str == "+" )
        return parent_sl + 1;
    else if ( expl_sl_str == "-" )
        return parent_sl - 1;
    else
    {
        qWarning() << "QwtMmlNode::scriptlevel(): could not parse value: \"" + expl_sl_str + "\"";
        return parent_sl;
    }
}

QPointF QwtMmlNode::devicePoint( const QPointF &pos ) const
{
    QRectF dr = deviceRect();

    if ( m_stretched )
    {
        return dr.topLeft() + QPointF( ( pos.x() - m_my_rect.left() ) * dr.width() / m_my_rect.width(),
                                       ( pos.y() - m_my_rect.top() ) * dr.height() / m_my_rect.height() );
    }
    else
    {
        return dr.topLeft() + pos - m_my_rect.topLeft();
    }
}

QString QwtMmlNode::inheritAttributeFromMrow( const QString &name,
                                              const QString &def ) const
{
    const QwtMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p == this || p->nodeType() == MstyleNode )
        {
            QString value = p->explicitAttribute( name );
            if ( !value.isNull() )
                return value;
        }
    }

    return def;
}

QColor QwtMmlNode::color() const
{
    // If we are child of <merror> return red
    const QwtMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p->nodeType() == MerrorNode )
            return QColor( "red" );
    }

    QString value_str = inheritAttributeFromMrow( "mathcolor" );
    if ( value_str.isNull() )
        value_str = inheritAttributeFromMrow( "color" );
    if ( value_str.isNull() )
        return QColor();

    return QColor( value_str );
}

QColor QwtMmlNode::background() const
{
    QString value_str = inheritAttributeFromMrow( "mathbackground" );
    if ( value_str.isNull() )
        value_str = inheritAttributeFromMrow( "background" );
    if ( value_str.isNull() )
        return QColor();

    return QColor( value_str );
}

static void updateFontAttr( QwtMmlAttributeMap &font_attr, const QwtMmlNode *n,
                            const QString &name,
                            const QString &preferred_name = QString() )
{
    if ( font_attr.contains( preferred_name ) || font_attr.contains( name ) )
        return;
    QString value = n->explicitAttribute( name );
    if ( !value.isNull() )
        font_attr[name] = value;
}

static QwtMmlAttributeMap collectFontAttributes( const QwtMmlNode *node )
{
    QwtMmlAttributeMap font_attr;

    for ( const QwtMmlNode *n = node; n != 0; n = n->parent() )
    {
        if ( n == node || n->nodeType() == QwtMml::MstyleNode )
        {
            updateFontAttr( font_attr, n, "mathvariant" );
            updateFontAttr( font_attr, n, "mathsize" );

            // depreciated attributes
            updateFontAttr( font_attr, n, "fontsize", "mathsize" );
            updateFontAttr( font_attr, n, "fontweight", "mathvariant" );
            updateFontAttr( font_attr, n, "fontstyle", "mathvariant" );
            updateFontAttr( font_attr, n, "fontfamily", "mathvariant" );
        }
    }

    return font_attr;
}

QFont QwtMmlNode::font() const
{
    QFont fn( m_document->fontName( QwtMathMLDocument::NormalFont ) );
    fn.setPointSizeF( m_document->baseFontPointSize() );

    qreal ps = fn.pointSizeF();
    int sl = scriptlevel();
    if ( sl >= 0 )
    {
        for ( int i = 0; i < sl; ++i )
            ps = ps * g_script_size_multiplier;
    }
    else
    {
        for ( int i = 0; i > sl; --i )
            ps = ps / g_script_size_multiplier;
    }
    if ( ps < g_min_font_point_size )
        ps = g_min_font_point_size;
    fn.setPointSize( ps );

    const qreal em = QFontMetricsF( fn ).boundingRect( 'm' ).width();
    const qreal ex = QFontMetricsF( fn ).boundingRect( 'x' ).height();

    QwtMmlAttributeMap font_attr = collectFontAttributes( this );

    if ( font_attr.contains( "mathvariant" ) )
    {
        QString value = font_attr["mathvariant"];

        bool ok;
        int mv = mmlInterpretMathVariant( value, &ok );

        if ( ok )
        {
            if ( mv & ScriptMV )
                fn.setFamily( m_document->fontName( QwtMathMLDocument::ScriptFont ) );

            if ( mv & FrakturMV )
                fn.setFamily( m_document->fontName( QwtMathMLDocument::FrakturFont ) );

            if ( mv & SansSerifMV )
                fn.setFamily( m_document->fontName( QwtMathMLDocument::SansSerifFont ) );

            if ( mv & MonospaceMV )
                fn.setFamily( m_document->fontName( QwtMathMLDocument::MonospaceFont ) );

            if ( mv & DoubleStruckMV )
                fn.setFamily( m_document->fontName( QwtMathMLDocument::DoublestruckFont ) );

            if ( mv & BoldMV )
                fn.setBold( true );

            if ( mv & ItalicMV )
                fn.setItalic( true );
        }
    }

    if ( font_attr.contains( "mathsize" ) )
    {
        QString value = font_attr["mathsize"];
        fn = mmlInterpretMathSize( value, fn, em, ex, 0 );
    }

    fn = mmlInterpretDepreciatedFontAttr( font_attr, fn, em, ex );

    if ( m_node_type == MiNode
            && !font_attr.contains( "mathvariant" )
            && !font_attr.contains( "fontstyle" ) )
    {
        const QwtMmlMiNode *mi_node = ( const QwtMmlMiNode* ) this;
        if ( mi_node->text().length() == 1 )
            fn.setItalic( true );
    }

    if ( m_node_type == MoNode )
    {
        fn.setItalic( false );
        fn.setBold( false );
    }

    return fn;
}

QString QwtMmlNode::explicitAttribute( const QString &name, const QString &def ) const
{
    QwtMmlAttributeMap::const_iterator it = m_attribute_map.find( name );
    if ( it != m_attribute_map.end() )
        return *it;
    return def;
}


QRectF QwtMmlNode::parentRect() const
{
    if ( m_stretched )
        return m_parent_rect;

    return QRectF( m_rel_origin + m_my_rect.topLeft(), m_my_rect.size() );
}


void QwtMmlNode::stretchTo( const QRectF &rect )
{
    m_parent_rect = rect;
    m_stretched = true;
}

void QwtMmlNode::setRelOrigin( const QPointF &rel_origin )
{
    m_rel_origin = rel_origin + QPointF( -m_my_rect.left(), 0.0 );
    m_stretched = false;
}

void QwtMmlNode::updateMyRect()
{
    m_my_rect = symbolRect();
    QwtMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        m_my_rect |= child->parentRect();
}

void QwtMmlNode::layout()
{
    m_parent_rect = QRectF( 0.0, 0.0, 0.0, 0.0 );
    m_stretched = false;
    m_rel_origin = QPointF( 0.0, 0.0 );

    QwtMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        child->layout();

    layoutSymbol();

    updateMyRect();

    if ( m_parent == 0 )
        m_rel_origin = QPointF( 0.0, 0.0 );
}


QRectF QwtMmlNode::deviceRect() const
{
    if ( m_parent == 0 )
        return QRectF( m_rel_origin + m_my_rect.topLeft(), m_my_rect.size() );

    QRectF pdr = m_parent->deviceRect();
    QRectF pr = parentRect();
    QRectF pmr = m_parent->myRect();

    qreal scale_w = 0.0;
    if ( pmr.width() != 0.0 )
        scale_w = pdr.width() / pmr.width();
    qreal scale_h = 0.0;
    if ( pmr.height() != 0.0 )
        scale_h = pdr.height() / pmr.height();

    return QRectF( pdr.left() + ( pr.left() - pmr.left() ) * scale_w,
                   pdr.top()  + ( pr.top() - pmr.top() ) * scale_h,
                   pr.width() * scale_w,
                   pr.height() * scale_h );
}

void QwtMmlNode::layoutSymbol()
{
    // default behaves like an mrow

    // now lay them out in a neat row, aligning their origins to my origin
    qreal w = 0.0;
    QwtMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
    {
        child->setRelOrigin( QPointF( w, 0.0 ) );
        w += child->parentRect().width() + 1.0;
    }
}

void QwtMmlNode::paint(
    QPainter *painter, qreal x_scaling, qreal y_scaling )
{
    if ( !m_my_rect.isValid() )
        return;

    painter->save();

    QRectF d_rect = deviceRect();

    if ( m_stretched )
    {
        x_scaling *= d_rect.width() / m_my_rect.width();
        y_scaling *= d_rect.height() / m_my_rect.height();
    }

    if ( m_node_type != UnknownNode )
    {
        const QColor bg = background();
        if ( bg.isValid() )
            painter->fillRect( d_rect, bg );
        else
            painter->fillRect( d_rect, m_document->backgroundColor() );

        const QColor fg = color();
        if ( fg.isValid() )
            painter->setPen( QPen( fg, 1 ) );
        else
            painter->setPen( QPen( m_document->foregroundColor(), 1 ) );
    }

    QwtMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        child->paint( painter, x_scaling, y_scaling );

    if ( m_node_type != UnknownNode )
        paintSymbol( painter, x_scaling, y_scaling );

    painter->restore();
}

void QwtMmlNode::paintSymbol( QPainter *painter, qreal, qreal ) const
{

#ifdef MML_TEST
    QRectF d_rect = deviceRect();
    if ( m_document->drawFrames() && d_rect.isValid() )
    {
        painter->save();

        painter->setPen( QPen( Qt::red, 0 ) );

        painter->drawRect( d_rect );

        QPen pen = painter->pen();
        pen.setStyle( Qt::DotLine );
        painter->setPen( pen );

        const QPointF d_pos = devicePoint( QPointF() );

        painter->drawLine( QPointF( d_rect.left(), d_pos.y() ),
                           QPointF( d_rect.right(), d_pos.y() ) );

        painter->restore();
    }
#else
    Q_UNUSED( painter )
#endif
}

void QwtMmlNode::stretch()
{
    QwtMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        child->stretch();
}

QString QwtMmlTokenNode::text() const
{
    QString result;

    const QwtMmlNode *child = firstChild();
    for ( ; child != 0; child = child->nextSibling() )
    {
        if ( child->nodeType() != TextNode ) continue;
        if ( !result.isEmpty() )
            result += ' ';
        result += static_cast<const QwtMmlTextNode *>( child )->text();
    }

    return result;
}

QwtMmlNode *QwtMmlMfracNode::numerator() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

QwtMmlNode *QwtMmlMfracNode::denominator() const
{
    QwtMmlNode *node = numerator()->nextSibling();
    Q_ASSERT( node != 0 );
    return node;
}

QRectF QwtMmlMfracNode::symbolRect() const
{
    QRectF num_rect = numerator()->myRect();
    QRectF denom_rect = denominator()->myRect();
    qreal spacing = g_mfrac_spacing * ( num_rect.height() + denom_rect.height() );
    qreal my_width = qMax( num_rect.width(), denom_rect.width() ) + 2.0 * spacing;
    int line_thickness = qCeil( lineThickness() );

    return QRectF( -0.5 * ( my_width + line_thickness ), -0.5 * line_thickness,
                   my_width + line_thickness, line_thickness );
}

void QwtMmlMfracNode::layoutSymbol()
{
    QwtMmlNode *num = numerator();
    QwtMmlNode *denom = denominator();

    QRectF num_rect = num->myRect();
    QRectF denom_rect = denom->myRect();

    qreal spacing = g_mfrac_spacing * ( num_rect.height() + denom_rect.height() );
    int line_thickness = qCeil( lineThickness() );

    num->setRelOrigin( QPointF( -0.5 * num_rect.width(), - spacing - num_rect.bottom() - 0.5 * line_thickness ) );
    denom->setRelOrigin( QPointF( -0.5 * denom_rect.width(), spacing - denom_rect.top() + 0.5 * line_thickness ) );
}

static bool zeroLineThickness( const QString &s )
{
    if ( s.length() == 0 || !s[0].isDigit() )
        return false;

    for ( int i = 0; i < s.length(); ++i )
    {
        QChar c = s.at( i );
        if ( c.isDigit() && c != '0' )
            return false;
    }
    return true;
}

qreal QwtMmlMfracNode::lineThickness() const
{
    QString linethickness_str = inheritAttributeFromMrow( "linethickness", QString::number( 0.75 * lineWidth () ) );

    /* InterpretSpacing returns a qreal, which might be 0 even if the thickness
       is > 0, though very very small. That's ok, because we can set it to 1.
       However, we have to run this check if the line thickness really is zero */
    if ( !zeroLineThickness( linethickness_str ) )
    {
        bool ok;
        qreal line_thickness = interpretSpacing( linethickness_str, &ok );
        if ( !ok || !line_thickness )
            line_thickness = 1.0;

        return line_thickness;
    }
    else
    {
        return 0.0;
    }
}

void QwtMmlMfracNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    QwtMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    int line_thickness = qCeil( lineThickness() );

    if ( line_thickness != 0.0 )
    {
        painter->save();

        QPen pen = painter->pen();
        pen.setWidthF( line_thickness );
        painter->setPen( pen );

        QRectF s_rect = symbolRect();
        s_rect.moveTopLeft( devicePoint( s_rect.topLeft() ) );

        painter->drawLine( QPointF( s_rect.left() + 0.5 * line_thickness, s_rect.center().y() ),
                           QPointF( s_rect.right() - 0.5 * line_thickness, s_rect.center().y() ) );

        painter->restore();
    }
}

QwtMmlNode *QwtMmlRootBaseNode::base() const
{
    return firstChild();
}

QwtMmlNode *QwtMmlRootBaseNode::index() const
{
    QwtMmlNode *b = base();
    if ( b == 0 )
        return 0;
    return b->nextSibling();
}

int QwtMmlRootBaseNode::scriptlevel( const QwtMmlNode *child ) const
{
    int sl = QwtMmlNode::scriptlevel();

    QwtMmlNode *i = index();
    if ( child != 0 && child == i )
        return sl + 1;
    else
        return sl;
}

QRectF QwtMmlRootBaseNode::baseRect() const
{
    QwtMmlNode *b = base();
    if ( b == 0 )
        return QRectF( 0.0, 0.0, 1.0, 1.0 );
    else
        return b->myRect();
}

QRectF QwtMmlRootBaseNode::radicalRect() const
{
    return QFontMetricsF( font() ).boundingRect( QChar( g_radical ) );
}

qreal QwtMmlRootBaseNode::radicalMargin() const
{
    return g_mroot_base_margin * baseRect().height();
}

qreal QwtMmlRootBaseNode::radicalLineWidth() const
{
    return g_mroot_base_line * lineWidth();
}

QRectF QwtMmlRootBaseNode::symbolRect() const
{
    QRectF base_rect = baseRect();
    qreal radical_margin = radicalMargin();
    qreal radical_width = radicalRect().width();
    int radical_line_width = qCeil( radicalLineWidth() );

    return QRectF( -radical_width, base_rect.top() - radical_margin - radical_line_width,
                    radical_width + base_rect.width() + radical_margin, base_rect.height() + 2.0 * radical_margin + radical_line_width );
}

void QwtMmlRootBaseNode::layoutSymbol()
{
    QwtMmlNode *b = base();
    if ( b != 0 )
        b->setRelOrigin( QPointF( 0.0, 0.0 ) );

    QwtMmlNode *i = index();
    if ( i != 0 )
    {
        QRectF i_rect = i->myRect();
        i->setRelOrigin( QPointF( -0.33 * radicalRect().width() - i_rect.width(),
                                  -1.1 * i_rect.bottom() ) );
    }
}

void QwtMmlRootBaseNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    QwtMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    painter->save();

    QRectF s_rect = symbolRect();
    s_rect.moveTopLeft( devicePoint( s_rect.topLeft() ) );

    QRectF radical_rect = QFontMetricsF( font() ).boundingRect( QChar( g_radical ) );

    QRectF rect = s_rect;
    rect.adjust(  0.0, qCeil( radicalLineWidth() ),
                 -(rect.width() - radical_rect.width() ), 0.0 );

    painter->translate( rect.bottomLeft() );

    QPointF radical_points[ g_radical_points_size ];

    for ( int i = 0; i < g_radical_points_size; ++i )
    {
        radical_points[ i ].setX( radical_rect.width() * g_radical_points[ i ].x() );
        radical_points[ i ].setY( -rect.height() * g_radical_points[ i ].y() );
    }

    qreal x2 = radical_points[ 2 ].x();
    qreal y2 = radical_points[ 2 ].y();
    qreal x3 = radical_points[ 3 ].x();
    qreal y3 = radical_points[ 3 ].y();

    radical_points[ 4 ].setX( s_rect.width() );
    radical_points[ 5 ].setX( s_rect.width() );

    radical_points[ 3 ].setY( -s_rect.height() );
    radical_points[ 4 ].setY( -s_rect.height() );

    qreal new_y3 = radical_points[ 3 ].y();

    radical_points[ 3 ].setX( x2 + ( x3 - x2 ) * ( new_y3 - y2 ) / ( y3 - y2 ) );

    QBrush brush = painter->brush();
    brush.setColor( painter->pen().color() );
    brush.setStyle( Qt::SolidPattern );
    painter->setBrush( brush );

    painter->setRenderHint( QPainter::Antialiasing, true );

    painter->drawPolygon( radical_points, g_radical_points_size );

    painter->restore();
}

QwtMmlTextNode::QwtMmlTextNode( const QString &text, QwtMmlDocument *document )
    : QwtMmlNode( TextNode, document, QwtMmlAttributeMap() )
{
    m_text = text;
    // Trim whitespace from ends, but keep nbsp and thinsp
    m_text.remove( QRegExp( "^[^\\S\\x00a0\\x2009]+" ) );
    m_text.remove( QRegExp( "[^\\S\\x00a0\\x2009]+$" ) );
}

QString QwtMmlTextNode::toStr() const
{
    return QwtMmlNode::toStr() + " text=\"" + m_text + "\"";
}

bool QwtMmlTextNode::isInvisibleOperator() const
{
    return    m_text == QString( QChar( 0x61, 0x20 ) )  // &ApplyFunction;
           || m_text == QString( QChar( 0x62, 0x20 ) )  // &InvisibleTimes;
           || m_text == QString( QChar( 0x63, 0x20 ) )  // &InvisibleComma;
           || m_text == QString( QChar( 0x64, 0x20 ) ); // Invisible addition
}

void QwtMmlTextNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    QwtMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    if ( isInvisibleOperator() )
        return;

    painter->save();

    QPointF d_pos = devicePoint( QPointF() );
    QPointF s_pos = symbolRect().topLeft();

    painter->translate( d_pos + s_pos );
    painter->scale( x_scaling, y_scaling );
    painter->setFont( font() );

    painter->drawText( QPointF( 0.0, basePos() ) - s_pos, m_text );

    painter->restore();
}

QRectF QwtMmlTextNode::symbolRect() const
{
    QRectF br = isInvisibleOperator() ? QRectF() : QFontMetricsF( font() ).tightBoundingRect( m_text );
    br.translate( 0.0, basePos() );

    return br;
}

QwtMmlNode *QwtMmlSubsupBaseNode::base() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

QwtMmlNode *QwtMmlSubsupBaseNode::sscript() const
{
    QwtMmlNode *s = base()->nextSibling();
    Q_ASSERT( s != 0 );
    return s;
}

int QwtMmlSubsupBaseNode::scriptlevel( const QwtMmlNode *child ) const
{
    int sl = QwtMmlNode::scriptlevel();

    QwtMmlNode *s = sscript();
    if ( child != 0 && child == s )
        return sl + 1;
    else
        return sl;
}

void QwtMmlMsupNode::layoutSymbol()
{
    QwtMmlNode *b = base();
    QwtMmlNode *s = sscript();
    qreal subsup_spacing = interpretSpacing( g_subsup_spacing, 0 );
    qreal threshold = b->myRect().top() + 0.5 * ( b->myRect().height() - subsup_spacing );
    qreal shift = 0.0;

    if ( b->myRect().top() + s->myRect().bottom() > threshold )
        shift = threshold - ( b->myRect().top() + s->myRect().bottom() );

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );
    s->setRelOrigin( QPointF( subsup_spacing, b->myRect().top() + shift ) );
}

void QwtMmlMsubNode::layoutSymbol()
{
    QwtMmlNode *b = base();
    QwtMmlNode *s = sscript();
    qreal subsup_spacing = interpretSpacing( g_subsup_spacing, 0 );
    qreal threshold = b->myRect().top() + 0.5 * ( b->myRect().height() + subsup_spacing );
    qreal sub_shift = 0.0;

    if ( b->myRect().bottom() + s->myRect().top() < threshold )
        sub_shift = threshold - ( b->myRect().bottom() + s->myRect().top() );

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );
    s->setRelOrigin( QPointF( subsup_spacing, b->myRect().bottom() + sub_shift ) );
}

QwtMmlNode *QwtMmlMsubsupNode::base() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

QwtMmlNode *QwtMmlMsubsupNode::subscript() const
{
    QwtMmlNode *sub = base()->nextSibling();
    Q_ASSERT( sub != 0 );
    return sub;
}

QwtMmlNode *QwtMmlMsubsupNode::superscript() const
{
    QwtMmlNode *sup = subscript()->nextSibling();
    Q_ASSERT( sup != 0 );
    return sup;
}

void QwtMmlMsubsupNode::layoutSymbol()
{
    QwtMmlNode *b = base();
    QwtMmlNode *sub = subscript();
    QwtMmlNode *sup = superscript();
    qreal subsup_spacing = interpretSpacing( g_subsup_spacing, 0 );
    qreal sub_threshold = b->myRect().top() + 0.5 * ( b->myRect().height() + subsup_spacing );
    qreal sup_threshold = sub_threshold - subsup_spacing;
    qreal sub_shift = 0.0;
    qreal sup_shift = 0.0;

    if ( b->myRect().bottom() + sub->myRect().top() < sub_threshold )
        sub_shift = sub_threshold - ( b->myRect().bottom() + sub->myRect().top() );

    if ( b->myRect().top() + sup->myRect().bottom() > sup_threshold )
        sup_shift = sup_threshold - ( b->myRect().top() + sup->myRect().bottom() );

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );
    sub->setRelOrigin( QPointF( subsup_spacing, b->myRect().bottom() + sub_shift ) );
    sup->setRelOrigin( QPointF( subsup_spacing, b->myRect().top() + sup_shift ) );
}

int QwtMmlMsubsupNode::scriptlevel( const QwtMmlNode *child ) const
{
    int sl = QwtMmlNode::scriptlevel();

    QwtMmlNode *sub = subscript();
    QwtMmlNode *sup = superscript();

    if ( child != 0 && ( child == sup || child == sub ) )
        return sl + 1;
    else
        return sl;
}

QString QwtMmlMoNode::toStr() const
{
    return QwtMmlNode::toStr() + QString( " form=%1" ).arg( ( int )form() );
}

void QwtMmlMoNode::layoutSymbol()
{
    if ( firstChild() == 0 )
        return;

    firstChild()->setRelOrigin( QPointF( 0.0, 0.0 ) );

    if ( m_oper_spec == 0 )
        m_oper_spec = mmlFindOperSpec( !text().compare( "−" )?"-":text(), form() );
}

QwtMmlMoNode::QwtMmlMoNode( QwtMmlDocument *document,
                            const QwtMmlAttributeMap &attribute_map )
    : QwtMmlTokenNode( MoNode, document, attribute_map )
{
    m_oper_spec = 0;
}

QString QwtMmlMoNode::dictionaryAttribute( const QString &name ) const
{
    const QwtMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p == this || p->nodeType() == MstyleNode )
        {
            QString expl_attr = p->explicitAttribute( name );
            if ( !expl_attr.isNull() )
                return expl_attr;
        }
    }

    return mmlDictAttribute( name, m_oper_spec );
}

bool QwtMmlMoNode::unaryMinus() const
{
    return !text().compare( "−" )
            && previousSibling() != 0
            && previousSibling()->nodeType() == MoNode
            && ( !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "=" )
                 || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "(" )
                 || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "|" )
                 || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "⌊" )
                 || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "⌈" )
                 || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "," ) );
}

QwtMml::FormType QwtMmlMoNode::form() const
{
    QString value_str = inheritAttributeFromMrow( "form" );
    if ( !value_str.isNull() )
    {
        bool ok;
        FormType value = mmlInterpretForm( value_str, &ok );
        if ( ok )
            return value;
        else
            qWarning() << "Could not convert " << value_str << " to form";
    }

    // Default heuristic.
    if ( firstSibling() == ( QwtMmlNode* )this && lastSibling() != ( QwtMmlNode* )this )
        return PrefixForm;
    else if ( lastSibling() == ( QwtMmlNode* )this && firstSibling() != ( QwtMmlNode* )this )
        return PostfixForm;
    else if ( unaryMinus() )
        return PrefixForm;
    else
        return InfixForm;
}

void QwtMmlMoNode::stretch()
{
    if ( parent() == 0 )
        return;

    if ( m_oper_spec == 0 )
        return;

    if ( m_oper_spec->stretch_dir == QwtMmlOperSpec::HStretch
            && parent()->nodeType() == MrowNode
            && ( previousSibling() != 0 || nextSibling() != 0) )
        return;

    QRectF pmr = parent()->myRect();
    QRectF pr = parentRect();

    switch ( m_oper_spec->stretch_dir )
    {
        case QwtMmlOperSpec::VStretch:
            stretchTo( QRectF( pr.left(), pmr.top(), pr.width(), pmr.height() ) );
            break;
        case QwtMmlOperSpec::HStretch:
            stretchTo( QRectF( pmr.left(), pr.top(), pmr.width(), pr.height() ) );
            break;
        case QwtMmlOperSpec::HVStretch:
            stretchTo( pmr );
            break;
        case QwtMmlOperSpec::NoStretch:
            break;
    }
}

qreal QwtMmlMoNode::lspace() const
{
    Q_ASSERT( m_oper_spec != 0 );
    if ( parent() == 0
            || ( parent()->nodeType() != MrowNode
                 && parent()->nodeType() != MfencedNode
                 && parent()->nodeType() != UnknownNode )
            || previousSibling() == 0
            || ( previousSibling() == 0 && nextSibling() == 0 )
            || ( previousSibling() != 0
                 && previousSibling()->nodeType() == MoNode
                 && !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( text() ) )
            || unaryMinus()
            || ( !text().compare( "|" )
                 && previousSibling() != 0
                 && previousSibling()->nodeType() == MoNode
                 && ( !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "=" )
                      || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "−" )
                      || !( ( QwtMmlMoNode* ) previousSibling() )->text().compare( "," ) ) ) )
        return 0.0;
    else
        return interpretSpacing( dictionaryAttribute( "lspace" ), 0 );
}

qreal QwtMmlMoNode::rspace() const
{
    Q_ASSERT( m_oper_spec != 0 );
    if ( parent() == 0
            || ( parent()->nodeType() != MrowNode
                 && parent()->nodeType() != MfencedNode
                 && parent()->nodeType() != UnknownNode )
            || nextSibling() == 0
            || ( previousSibling() == 0 && nextSibling() == 0 ) )
        return 0.0;
    else
        return interpretSpacing( dictionaryAttribute( "rspace" ), 0 );
}

QRectF QwtMmlMoNode::symbolRect() const
{
    if ( firstChild() == 0 )
        return QRectF( 0.0, 0.0, 0.0, 0.0 );

    QRectF cmr = firstChild()->myRect();

    return QRectF( -lspace(), cmr.top(),
                   cmr.width() + lspace() + rspace(), cmr.height() );
}

qreal QwtMmlMtableNode::rowspacing() const
{
    QString value = explicitAttribute( "rowspacing" );
    if ( value.isNull() )
        return ex();
    bool ok;
    qreal spacing = interpretSpacing( value, &ok );

    if ( ok )
        return spacing;
    else
        return ex();
}

qreal QwtMmlMtableNode::columnspacing() const
{
    QString value = explicitAttribute( "columnspacing" );
    if ( value.isNull() )
        return 0.8 * em();
    bool ok;
    qreal spacing = interpretSpacing( value, &ok );

    if ( ok )
        return spacing;
    else
        return 0.8 * em();
}

qreal QwtMmlMtableNode::CellSizeData::colWidthSum() const
{
    qreal w = 0.0;
    for ( int i = 0; i < col_widths.count(); ++i )
        w += col_widths[i];
    return w;
}

qreal QwtMmlMtableNode::CellSizeData::rowHeightSum() const
{
    qreal h = 0.0;
    for ( int i = 0; i < row_heights.count(); ++i )
        h += row_heights[i];
    return h;
}

void QwtMmlMtableNode::CellSizeData::init( const QwtMmlNode *first_row )
{
    col_widths.clear();
    row_heights.clear();

    const QwtMmlNode *mtr = first_row;
    for ( ; mtr != 0; mtr = mtr->nextSibling() )
    {
        Q_ASSERT( mtr->nodeType() == MtrNode );

        int col_cnt = 0;
        const QwtMmlNode *mtd = mtr->firstChild();
        for ( ; mtd != 0; mtd = mtd->nextSibling(), ++col_cnt )
        {
            Q_ASSERT( mtd->nodeType() == MtdNode );

            QRectF mtdmr = mtd->myRect();

            if ( col_cnt == col_widths.count() )
                col_widths.append( mtdmr.width() );
            else
                col_widths[col_cnt] = qMax( col_widths[col_cnt], mtdmr.width() );
        }

        row_heights.append( mtr->myRect().height() );
    }
}

static qreal mmlQstringToQreal( const QString &string, bool *ok )
{
    static const int SizeOfDouble = sizeof( double );

    if ( sizeof( qreal ) == SizeOfDouble )
        return string.toDouble( ok );
    else
        return string.toFloat( ok );
}

void QwtMmlMtableNode::layoutSymbol()
{
    // Obtain natural widths of columns
    m_cell_size_data.init( firstChild() );

    qreal col_spc = columnspacing();
    qreal row_spc = rowspacing();
    qreal frame_spc_hor = framespacing_hor();
    QString columnwidth_attr = explicitAttribute( "columnwidth", "auto" );

    // Is table width set by user? If so, set col_width_sum and never ever change it.
    qreal col_width_sum = m_cell_size_data.colWidthSum();
    bool width_set_by_user = false;
    QString width_str = explicitAttribute( "width", "auto" );
    if ( width_str != "auto" )
    {
        bool ok;

        qreal w = interpretSpacing( width_str, &ok );
        if ( ok )
        {
            col_width_sum = w
                            - col_spc * ( m_cell_size_data.numCols() - 1 )
                            - frame_spc_hor * 2.0;
            width_set_by_user = true;
        }
    }

    // Find out what kind of columns we are dealing with and set the widths of
    // statically sized columns.
    qreal fixed_width_sum = 0.0;          // sum of widths of statically sized set columns
    qreal auto_width_sum = 0.0;           // sum of natural widths of auto sized columns
    qreal relative_width_sum = 0.0;       // sum of natural widths of relatively sized columns
    qreal relative_fraction_sum = 0.0;    // total fraction of width taken by relatively
    // sized columns
    int i;
    for ( i = 0; i < m_cell_size_data.numCols(); ++i )
    {
        QString value = mmlInterpretListAttr( columnwidth_attr, i, "auto" );

        // Is it an auto sized column?
        if ( value == "auto" || value == "fit" )
        {
            auto_width_sum += m_cell_size_data.col_widths[i];
            continue;
        }

        // Is it a statically sized column?
        bool ok;
        qreal w = interpretSpacing( value, &ok );
        if ( ok )
        {
            // Yup, sets its width to the user specified value
            m_cell_size_data.col_widths[i] = w;
            fixed_width_sum += w;
            continue;
        }

        // Is it a relatively sized column?
        if ( value.endsWith( "%" ) )
        {
            value.truncate( value.length() - 1 );
            qreal factor = mmlQstringToQreal( value, &ok );
            if ( ok && !value.isEmpty() )
            {
                factor *= 0.01;
                relative_width_sum += m_cell_size_data.col_widths[i];
                relative_fraction_sum += factor;
                if ( !width_set_by_user )
                {
                    // If the table width was not set by the user, we are free to increase
                    // it so that the width of this column will be >= than its natural width
                    qreal min_col_width_sum = m_cell_size_data.col_widths[i] / factor;
                    if ( min_col_width_sum > col_width_sum )
                        col_width_sum = min_col_width_sum;
                }
                continue;
            }
            else
                qWarning() << "QwtMmlMtableNode::layoutSymbol(): could not parse value " << value << "%%";
        }

        // Relatively sized column, but we failed to parse the factor. Treat is like an auto
        // column.
        auto_width_sum += m_cell_size_data.col_widths[i];
    }

    // Work out how much space remains for the auto olumns, after allocating
    // the statically sized and the relatively sized columns.
    qreal required_auto_width_sum = col_width_sum
                                    - relative_fraction_sum * col_width_sum
                                    - fixed_width_sum;

    if ( !width_set_by_user && required_auto_width_sum < auto_width_sum )
    {
#if 1
        if ( relative_fraction_sum < 1.0 )
            col_width_sum = ( fixed_width_sum + auto_width_sum ) / ( 1.0 - relative_fraction_sum );
        else
            col_width_sum = fixed_width_sum + auto_width_sum + relative_width_sum;
#endif
        required_auto_width_sum = auto_width_sum;
    }

    // Ratio by which we have to shring/grow all auto sized columns to make it all fit
    qreal auto_width_scale = 1.0;
    if ( auto_width_sum > 0.0 )
        auto_width_scale = required_auto_width_sum / auto_width_sum;

    // Set correct sizes for the auto sized and the relatively sized columns.
    for ( i = 0; i < m_cell_size_data.numCols(); ++i )
    {
        QString value = mmlInterpretListAttr( columnwidth_attr, i, "auto" );

        // Is it a relatively sized column?
        if ( value.endsWith( "%" ) )
        {
            bool ok;
            qreal w = mmlInterpretPercentSpacing( value, col_width_sum, &ok );
            if ( ok )
            {
                m_cell_size_data.col_widths[i] = w;
            }
            else
            {
                // We're treating parsing errors here as auto sized columns
                m_cell_size_data.col_widths[i] = auto_width_scale * m_cell_size_data.col_widths[i];
            }
        }
        // Is it an auto sized column?
        else if ( value == "auto" )
        {
            m_cell_size_data.col_widths[i] = auto_width_scale * m_cell_size_data.col_widths[i];
        }
    }

    m_content_width = m_cell_size_data.colWidthSum()
                      + col_spc * ( m_cell_size_data.numCols() - 1 );
    m_content_height = m_cell_size_data.rowHeightSum()
                       + row_spc * ( m_cell_size_data.numRows() - 1 );

    qreal bottom = -0.5 * m_content_height;
    QwtMmlNode *child = firstChild();
    for ( ; child != 0; child = child->nextSibling() )
    {
        Q_ASSERT( child->nodeType() == MtrNode );
        QwtMmlMtrNode *row = ( QwtMmlMtrNode* ) child;

        row->layoutCells( m_cell_size_data.col_widths, col_spc );
        QRectF rmr = row->myRect();
        row->setRelOrigin( QPointF( 0.0, bottom - rmr.top() ) );
        bottom += rmr.height() + row_spc;
    }
}

QRectF QwtMmlMtableNode::symbolRect() const
{
    qreal frame_spc_hor = framespacing_hor();
    qreal frame_spc_ver = framespacing_ver();

    return QRectF( -frame_spc_hor,
                   -0.5 * m_content_height - frame_spc_ver,
                   m_content_width + 2.0 * frame_spc_hor,
                   m_content_height + 2.0 * frame_spc_ver );
}

QwtMml::FrameType QwtMmlMtableNode::frame() const
{
    QString value = explicitAttribute( "frame", "none" );
    return mmlInterpretFrameType( value, 0, 0 );
}

QwtMml::FrameType QwtMmlMtableNode::columnlines( int idx ) const
{
    QString value = explicitAttribute( "columnlines", "none" );
    return mmlInterpretFrameType( value, idx, 0 );
}

QwtMml::FrameType QwtMmlMtableNode::rowlines( int idx ) const
{
    QString value = explicitAttribute( "rowlines", "none" );
    return mmlInterpretFrameType( value, idx, 0 );
}

void QwtMmlMtableNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    QwtMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    painter->save();

    painter->translate( devicePoint( QPointF() ) );

    QPen pen = painter->pen();

    FrameType frame_type = frame();
    if ( frame_type != FrameNone )
    {
        if ( frame_type == FrameDashed )
            pen.setStyle( Qt::DashLine );
        else
            pen.setStyle( Qt::SolidLine );
        painter->setPen( pen );
        painter->drawRect( myRect() );
    }

    qreal col_spc = columnspacing();
    qreal row_spc = rowspacing();

    qreal col_offset = 0.0;
    int i;
    for ( i = 0; i < m_cell_size_data.numCols() - 1; ++i )
    {
        FrameType frame_type = columnlines( i );
        col_offset += m_cell_size_data.col_widths[i];

        if ( frame_type != FrameNone )
        {
            if ( frame_type == FrameDashed )
                pen.setStyle( Qt::DashLine );
            else if ( frame_type == FrameSolid )
                pen.setStyle( Qt::SolidLine );

            painter->setPen( pen );
            qreal x = col_offset + 0.5 * col_spc;
            painter->drawLine( QPointF( x, -0.5 * m_content_height ),
                               QPointF( x,  0.5 * m_content_height ) );
        }
        col_offset += col_spc;
    }

    qreal row_offset = 0.0;
    for ( i = 0; i < m_cell_size_data.numRows() - 1; ++i )
    {
        FrameType frame_type = rowlines( i );
        row_offset += m_cell_size_data.row_heights[i];

        if ( frame_type != FrameNone )
        {
            if ( frame_type == FrameDashed )
                pen.setStyle( Qt::DashLine );
            else if ( frame_type == FrameSolid )
                pen.setStyle( Qt::SolidLine );

            painter->setPen( pen );
            qreal y = row_offset + 0.5 * ( row_spc - m_content_height );
            painter->drawLine( QPointF( 0, y ),
                               QPointF( m_content_width, y ) );
        }
        row_offset += row_spc;
    }

    painter->restore();
}

qreal QwtMmlMtableNode::framespacing_ver() const
{
    if ( frame() == FrameNone )
        return 0.2 * em();

    QString value = explicitAttribute( "framespacing", "0.4em 0.5ex" );

    bool ok;
    FrameSpacing fs = mmlInterpretFrameSpacing( value, em(), ex(), &ok );
    if ( ok )
        return fs.m_ver;
    else
        return 0.5 * ex();
}

qreal QwtMmlMtableNode::framespacing_hor() const
{
    if ( frame() == FrameNone )
        return 0.2 * em();

    QString value = explicitAttribute( "framespacing", "0.4em 0.5ex" );

    bool ok;
    FrameSpacing fs = mmlInterpretFrameSpacing( value, em(), ex(), &ok );
    if ( ok )
        return fs.m_hor;
    else
        return 0.4 * em();
}

void QwtMmlMtrNode::layoutCells( const QList<qreal> &col_widths,
                                 qreal col_spc )
{
    const QRectF mr = myRect();

    QwtMmlNode *child = firstChild();
    qreal col_offset = 0.0;
    int colnum = 0;
    for ( ; child != 0; child = child->nextSibling(), ++colnum )
    {
        Q_ASSERT( child->nodeType() == MtdNode );
        QwtMmlMtdNode *mtd = ( QwtMmlMtdNode* ) child;

        QRectF rect = QRectF( 0.0, mr.top(), col_widths[colnum], mr.height() );
        mtd->setMyRect( rect );
        mtd->setRelOrigin( QPointF( col_offset, 0.0 ) );
        col_offset += col_widths[colnum] + col_spc;
    }

    updateMyRect();
}

int QwtMmlMtdNode::scriptlevel( const QwtMmlNode *child ) const
{
    int sl = QwtMmlNode::scriptlevel();
    if ( child != 0 && child == firstChild() )
        return sl + m_scriptlevel_adjust;
    else
        return sl;
}

void QwtMmlMtdNode::setMyRect( const QRectF &rect )
{
    QwtMmlNode::setMyRect( rect );
    QwtMmlNode *child = firstChild();
    if ( child == 0 )
        return;

    if ( rect.width() < child->myRect().width() )
    {
        while ( rect.width() < child->myRect().width()
                && child->font().pointSize() > g_min_font_point_size )
        {
            qWarning() << "QwtMmlMtdNode::setMyRect(): rect.width()=" << rect.width()
                       << ", child->myRect().width=" << child->myRect().width()
                       << " sl=" << m_scriptlevel_adjust;

            ++m_scriptlevel_adjust;
            child->layout();
        }

        qWarning() << "QwtMmlMtdNode::setMyRect(): rect.width()=" << rect.width()
                   << ", child->myRect().width=" << child->myRect().width()
                   << " sl=" << m_scriptlevel_adjust;
    }

    QRectF mr = myRect();
    QRectF cmr = child->myRect();

    QPointF child_rel_origin;

    switch ( columnalign() )
    {
        case ColAlignLeft:
            child_rel_origin.setX( 0.0 );
            break;
        case ColAlignCenter:
            child_rel_origin.setX( mr.left() + 0.5 * ( mr.width() - cmr.width() ) );
            break;
        case ColAlignRight:
            child_rel_origin.setX( mr.right() - cmr.width() );
            break;
    }

    switch ( rowalign() )
    {
        case RowAlignTop:
            child_rel_origin.setY( mr.top() - cmr.top() );
            break;
        case RowAlignCenter:
        case RowAlignBaseline:
            child_rel_origin.setY( mr.top() - cmr.top() + 0.5 * ( mr.height() - cmr.height() ) );
            break;
        case RowAlignBottom:
            child_rel_origin.setY( mr.bottom() - cmr.bottom() );
            break;
        case RowAlignAxis:
            child_rel_origin.setY( 0.0 );
            break;
    }

    child->setRelOrigin( child_rel_origin );
}

int QwtMmlMtdNode::colNum() const
{
    QwtMmlNode *syb = previousSibling();

    int i = 0;
    for ( ; syb != 0; syb = syb->previousSibling() )
        ++i;

    return i;
}

int QwtMmlMtdNode::rowNum() const
{
    QwtMmlNode *row = parent()->previousSibling();

    int i = 0;
    for ( ; row != 0; row = row->previousSibling() )
        ++i;

    return i;
}

QwtMmlMtdNode::ColAlign QwtMmlMtdNode::columnalign()
{
    QString val = explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, 0, 0 );

    QwtMmlNode *node = parent(); // <mtr>
    if ( node == 0 )
        return ColAlignCenter;

    int colnum = colNum();
    val = node->explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, colnum, 0 );

    node = node->parent(); // <mtable>
    if ( node == 0 )
        return ColAlignCenter;

    val = node->explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, colnum, 0 );

    return ColAlignCenter;
}

QwtMmlMtdNode::RowAlign QwtMmlMtdNode::rowalign()
{
    QString val = explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, 0, 0 );

    QwtMmlNode *node = parent(); // <mtr>
    if ( node == 0 )
        return RowAlignAxis;

    int rownum = rowNum();
    val = node->explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, rownum, 0 );

    node = node->parent(); // <mtable>
    if ( node == 0 )
        return RowAlignAxis;

    val = node->explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, rownum, 0 );

    return RowAlignAxis;
}

void QwtMmlMoverNode::layoutSymbol()
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *over = base->nextSibling();
    Q_ASSERT( over != 0 );

    QRectF base_rect = base->myRect();
    QRectF over_rect = over->myRect();

    qreal spacing = explicitAttribute( "accent" ) == "true" ? 0.0 : g_mfrac_spacing * ( over_rect.height() + base_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal over_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    over->setRelOrigin( QPointF( -over_rel_factor * over_rect.width(),
                                 base_rect.top() - spacing - over_rect.bottom() ) );
}

int QwtMmlMoverNode::scriptlevel( const QwtMmlNode *node ) const
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *over = base->nextSibling();
    Q_ASSERT( over != 0 );

    int sl = QwtMmlNode::scriptlevel();
    if ( node != 0 && node == over )
        return sl + 1;
    else
        return sl;
}

void QwtMmlMunderNode::layoutSymbol()
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );

    QRectF base_rect = base->myRect();
    QRectF under_rect = under->myRect();

    qreal spacing = explicitAttribute( "accentunder" ) == "true" ? 0.0 : g_mfrac_spacing * ( under_rect.height() + base_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal under_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    under->setRelOrigin( QPointF( -under_rel_factor * under_rect.width(), base_rect.bottom() + spacing - under_rect.top() ) );
}

int QwtMmlMunderNode::scriptlevel( const QwtMmlNode *node ) const
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );

    int sl = QwtMmlNode::scriptlevel();
    if ( node != 0 && node == under )
        return sl + 1;
    else
        return sl;
}

void QwtMmlMunderoverNode::layoutSymbol()
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );
    QwtMmlNode *over = under->nextSibling();
    Q_ASSERT( over != 0 );

    QRectF base_rect = base->myRect();
    QRectF under_rect = under->myRect();
    QRectF over_rect = over->myRect();

    qreal over_spacing = explicitAttribute( "accent" ) == "true" ? 0.0 : g_mfrac_spacing * ( base_rect.height() + under_rect.height() + over_rect.height() );
    qreal under_spacing = explicitAttribute( "accentunder" ) == "true" ? 0.0 : g_mfrac_spacing * ( base_rect.height() + under_rect.height() + over_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal underover_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    under->setRelOrigin( QPointF( -underover_rel_factor * under_rect.width(), base_rect.bottom() + under_spacing - under_rect.top() ) );
    over->setRelOrigin( QPointF( -underover_rel_factor * over_rect.width(), base_rect.top() - over_spacing - under_rect.bottom() ) );
}

int QwtMmlMunderoverNode::scriptlevel( const QwtMmlNode *node ) const
{
    QwtMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    QwtMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );
    QwtMmlNode *over = under->nextSibling();
    Q_ASSERT( over != 0 );

    int sl = QwtMmlNode::scriptlevel();
    if ( node != 0 && ( node == under || node == over ) )
        return sl + 1;
    else
        return sl;
}

qreal QwtMmlSpacingNode::interpretSpacing( QString value, qreal base_value,
                                           bool *ok ) const
{
    if ( ok != 0 )
        *ok = false;

    value.replace( ' ', "" );

    QString sign, factor_str, pseudo_unit;
    bool percent = false;

    // extract the sign
    int idx = 0;
    if ( idx < value.length() && ( value.at( idx ) == '+' || value.at( idx ) == '-' ) )
        sign = value.at( idx++ );

    // extract the factor
    while ( idx < value.length() && ( value.at( idx ).isDigit() || value.at( idx ) == '.' ) )
        factor_str.append( value.at( idx++ ) );

    if ( factor_str == "" )
        factor_str = "1.0";

    // extract the % sign
    if ( idx < value.length() && value.at( idx ) == '%' )
    {
        percent = true;
        ++idx;
    }

    // extract the pseudo-unit
    pseudo_unit = value.mid( idx );

    bool qreal_ok;
    qreal factor = mmlQstringToQreal( factor_str, &qreal_ok );
    if ( !qreal_ok || factor < 0.0 )
    {
        qWarning( "QwtMmlSpacingNode::interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
        return 0.0;
    }

    if ( percent )
        factor *= 0.01;

    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    qreal unit_size;

    if ( pseudo_unit.isEmpty() )
        unit_size = base_value;
    else if ( pseudo_unit == "width" )
        unit_size = cr.width();
    else if ( pseudo_unit == "height" )
        unit_size = -cr.top();
    else if ( pseudo_unit == "depth" )
        unit_size = cr.bottom();
    else
    {
        bool unit_ok;

        if (    pseudo_unit == "em" || pseudo_unit == "ex"
             || pseudo_unit == "cm" || pseudo_unit == "mm"
             || pseudo_unit == "in" || pseudo_unit == "px" )
            unit_size = QwtMmlNode::interpretSpacing( "1" + pseudo_unit, &unit_ok );
        else
            unit_size = QwtMmlNode::interpretSpacing( pseudo_unit, &unit_ok );

        if ( !unit_ok )
        {
            qWarning( "QwtMmlSpacingNode::interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
            return 0.0;
        }
    }

    if ( ok != 0 )
        *ok = true;

    if ( sign.isNull() )
        return factor * unit_size;
    else if ( sign == "+" )
        return base_value + factor * unit_size;
    else // sign == "-"
        return base_value - factor * unit_size;
}

qreal QwtMmlSpacingNode::width() const
{
    qreal child_width = 0.0;
    if ( firstChild() != 0 )
        child_width = firstChild()->myRect().width();

    QString value = explicitAttribute( "width" );
    if ( value.isNull() )
        return child_width;

    bool ok;
    qreal w = interpretSpacing( value, child_width, &ok );
    if ( ok )
        return w;

    return child_width;
}

qreal QwtMmlSpacingNode::height() const
{
    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    QString value = explicitAttribute( "height" );
    if ( value.isNull() )
        return -cr.top();

    bool ok;
    qreal h = interpretSpacing( value, -cr.top(), &ok );
    if ( ok )
        return h;

    return -cr.top();
}

qreal QwtMmlSpacingNode::depth() const
{
    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    QString value = explicitAttribute( "depth" );
    if ( value.isNull() )
        return cr.bottom();

    bool ok;
    qreal h = interpretSpacing( value, cr.bottom(), &ok );
    if ( ok )
        return h;

    return cr.bottom();
}

void QwtMmlSpacingNode::layoutSymbol()
{
    if ( firstChild() == 0 )
        return;

    firstChild()->setRelOrigin( QPointF( 0.0, 0.0 ) );
}


QRectF QwtMmlSpacingNode::symbolRect() const
{
    return QRectF( 0.0, -height(), width(), height() + depth() );
}

qreal QwtMmlMpaddedNode::lspace() const
{
    QString value = explicitAttribute( "lspace" );

    if ( value.isNull() )
        return 0.0;

    bool ok;
    qreal lspace = interpretSpacing( value, 0.0, &ok );

    if ( ok )
        return lspace;

    return 0.0;
}

QRectF QwtMmlMpaddedNode::symbolRect() const
{
    return QRectF( -lspace(), -height(), lspace() + width(), height() + depth() );
}

// *******************************************************************
// Static helper functions
// *******************************************************************

static qreal mmlInterpretSpacing( QString value, qreal em, qreal ex, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "thin" )
        return 1.0;

    if ( value == "medium" )
        return 2.0;

    if ( value == "thick" )
        return 3.0;

    struct HSpacingValue
    {
        QString name;
        qreal factor;
    };

    static const HSpacingValue g_h_spacing_data[] =
    {
        { "veryverythinmathspace",  0.0555556 },
        { "verythinmathspace",      0.111111  },
        { "thinmathspace",          0.166667  },
        { "mediummathspace",        0.222222  },
        { "thickmathspace",         0.277778  },
        { "verythickmathspace",     0.333333  },
        { "veryverythickmathspace", 0.388889  },
        { 0,                        0.0       }
    };

    const HSpacingValue *v = g_h_spacing_data;
    for ( ; v->name != 0; ++v )
    {
        if ( value == v->name )
            return em * v->factor;
    }

    if ( value.endsWith( "em" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
            return em * factor;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sem\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "ex" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
            return ex * factor;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sex\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "cm" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            Q_ASSERT( qApp->desktop() != 0 );
            QDesktopWidget *dw = qApp->desktop();
            Q_ASSERT( dw->width() != 0 );
            Q_ASSERT( dw->widthMM() != 0 );
            return factor * 10.0 * dw->width() / dw->widthMM();
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%scm\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "mm" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            Q_ASSERT( qApp->desktop() != 0 );
            QDesktopWidget *dw = qApp->desktop();
            Q_ASSERT( dw->width() != 0 );
            Q_ASSERT( dw->widthMM() != 0 );
            return factor * dw->width() / dw->widthMM();
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%smm\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "in" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            Q_ASSERT( qApp->desktop() != 0 );
            QDesktopWidget *dw = qApp->desktop();
            Q_ASSERT( dw->width() != 0 );
            Q_ASSERT( dw->widthMM() != 0 );
            return factor * 10.0 * dw->width() / ( 2.54 * dw->widthMM() );
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sin\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "px" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        int i = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && i >= 0 )
            return i;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%spx\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    bool qreal_ok;
    int i = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && i >= 0 )
        return i;

    qWarning( "interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static qreal mmlInterpretPercentSpacing(
    QString value, qreal base, bool *ok )
{
    if ( !value.endsWith( "%" ) )
    {
        if ( ok != 0 )
            *ok = false;
        return 0.0;
    }

    value.truncate( value.length() - 1 );
    bool qreal_ok;
    qreal factor = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && factor >= 0.0 )
    {
        if ( ok != 0 )
            *ok = true;
        return 0.01 * base * factor;
    }

    qWarning( "interpretPercentSpacing(): could not parse \"%s%%\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static qreal mmlInterpretPointSize( QString value, bool *ok )
{
    if ( !value.endsWith( "pt" ) )
    {
        if ( ok != 0 )
            *ok = false;
        return 0;
    }

    value.truncate( value.length() - 2 );
    bool qreal_ok;
    qreal pt_size = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && pt_size > 0.0 )
    {
        if ( ok != 0 )
            *ok = true;
        return pt_size;
    }

    qWarning( "interpretPointSize(): could not parse \"%spt\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static const QwtMmlNodeSpec *mmlFindNodeSpec( QwtMml::NodeType type )
{
    const QwtMmlNodeSpec *spec = g_node_spec_data;
    for ( ; spec->type != QwtMml::NoNode; ++spec )
    {
        if ( type == spec->type ) return spec;
    }
    return 0;
}

static const QwtMmlNodeSpec *mmlFindNodeSpec( const QString &tag )
{
    const QwtMmlNodeSpec *spec = g_node_spec_data;
    for ( ; spec->type != QwtMml::NoNode; ++spec )
    {
        if ( tag == spec->tag ) return spec;
    }
    return 0;
}

static bool mmlCheckChildType( QwtMml::NodeType parent_type,
                               QwtMml::NodeType child_type,
                               QString *error_str )
{
    if ( parent_type == QwtMml::UnknownNode || child_type == QwtMml::UnknownNode )
        return true;

    const QwtMmlNodeSpec *child_spec = mmlFindNodeSpec( child_type );
    const QwtMmlNodeSpec *parent_spec = mmlFindNodeSpec( parent_type );

    Q_ASSERT( parent_spec != 0 );
    Q_ASSERT( child_spec != 0 );

    QString allowed_child_types( parent_spec->child_types );
    // null list means any child type is valid
    if ( allowed_child_types.isNull() )
        return true;

    QString child_type_str = QString( " " ) + child_spec->type_str + " ";
    if ( !allowed_child_types.contains( child_type_str ) )
    {
        if ( error_str != 0 )
            *error_str = QString( "illegal child " )
                         + child_spec->type_str
                         + " for parent "
                         + parent_spec->type_str;
        return false;
    }

    return true;
}

static bool mmlCheckAttributes( QwtMml::NodeType child_type,
                                const QwtMmlAttributeMap &attr,
                                QString *error_str )
{
    const QwtMmlNodeSpec *spec = mmlFindNodeSpec( child_type );
    Q_ASSERT( spec != 0 );

    QString allowed_attr( spec->attributes );
    // empty list means any attr is valid
    if ( allowed_attr.isEmpty() )
        return true;

    QwtMmlAttributeMap::const_iterator it = attr.begin(), end = attr.end();
    for ( ; it != end; ++it )
    {
        QString name = it.key();

        if ( name.indexOf( ':' ) != -1 )
            continue;

        QString padded_name = " " + name + " ";
        if ( !allowed_attr.contains( padded_name ) )
        {
            if ( error_str != 0 )
                *error_str = QString( "illegal attribute " )
                             + name
                             + " in "
                             + spec->type_str;
            return false;
        }
    }

    return true;
}

static int attributeIndex( const QString &name )
{
    for ( int i = 0; i < g_oper_spec_rows; ++i )
    {
        if ( name == g_oper_spec_names[i] )
            return i;
    }
    return -1;
}

struct OperSpecSearchResult
{
    OperSpecSearchResult() { prefix_form = infix_form = postfix_form = 0; }

    const QwtMmlOperSpec *prefix_form,
                         *infix_form,
                         *postfix_form;

    const QwtMmlOperSpec *&getForm( QwtMml::FormType form );
    bool haveForm( const QwtMml::FormType &form ) { return getForm( form ) != 0; }
    void addForm( const QwtMmlOperSpec *spec ) { getForm( spec->form ) = spec; }
};

const QwtMmlOperSpec *&OperSpecSearchResult::getForm( QwtMml::FormType form )
{
    switch ( form )
    {
        case QwtMml::PrefixForm:
            return prefix_form;
        case QwtMml::InfixForm:
            return infix_form;
        case QwtMml::PostfixForm:
            return postfix_form;
    }
    return postfix_form; // just to avoid warning
}

/*
    Searches g_oper_spec_data and returns any instance of operator name. There may
    be more instances, but since the list is sorted, they will be next to each other.
*/
static const QwtMmlOperSpec *searchOperSpecData( const QString &name )
{
    // binary search
    // establish invariant g_oper_spec_data[begin].name < name < g_oper_spec_data[end].name

    int cmp = name.compare( g_oper_spec_data[0].name );
    if ( cmp < 0 )
        return 0;

    if ( cmp == 0 )
        return g_oper_spec_data;

    int begin = 0;
    int end = g_oper_spec_count;

    // invariant holds
    while ( end - begin > 1 )
    {
        int mid = 0.5 * ( begin + end );

        const QwtMmlOperSpec *spec = g_oper_spec_data + mid;
        int cmp = name.compare( spec->name );
        if ( cmp < 0 )
            end = mid;
        else if ( cmp > 0 )
            begin = mid;
        else
            return spec;
    }

    return 0;
}

/*
    This searches g_oper_spec_data until at least one name in name_list is found with FormType form,
    or until name_list is exhausted. The idea here is that if we don't find the operator in the
    specified form, we still want to use some other available form of that operator.
*/
static OperSpecSearchResult _mmlFindOperSpec( const QStringList &name_list,
                                              QwtMml::FormType form )
{
    OperSpecSearchResult result;

    const QwtMmlOperSpec *firstSpec = 0;

    QStringList::const_iterator it = name_list.begin();
    for ( ; it != name_list.end(); ++it )
    {
        const QString &name = *it;

        const QwtMmlOperSpec *spec = searchOperSpecData( name );

        if ( spec == 0 )
            continue;

        // backtrack to the first instance of name
        while ( spec > g_oper_spec_data && ( spec - 1 )->name.compare( name ) == 0 )
            --spec;

        // Keep track of the first intance, if we haven't already done so

        if ( !firstSpec )
            firstSpec = spec;

        // iterate over instances of name until the instances are exhausted or until we
        // find an instance in the specified form.
        do
        {
            result.addForm( spec++ );
            if ( result.haveForm( form ) )
                break;
        }
        while ( spec->name.compare( name ) == 0 );

        if ( result.haveForm( form ) )
            break;
    }

    // Check whether we have found an instance in the specified form for one of
    // the different names in the given list. If not, and if there is more than
    // one name in the given list, then use our first instance, if any.

    if ( !result.haveForm( form ) && name_list.count() > 1 && firstSpec ) {
        const QString &name = firstSpec->name;

        do
        {
            result.addForm( firstSpec++ );
            if ( result.haveForm( form ) )
                break;
        }
        while ( firstSpec->name.compare( name ) == 0 );
    }

    return result;
}

/*
    text is a string between <mo> and </mo>. It can be a character ('+'), an
    entity reference ("&infin;") or a character reference ("&#x0221E"). Our
    job is to find an operator spec in the operator dictionary (g_oper_spec_data)
    that matches text. Things are further complicated by the fact, that many
    operators come in several forms (prefix, infix, postfix).

    If available, this function returns an operator spec matching text in the specified
    form. If such operator is not available, returns an operator spec that matches
    text, but of some other form in the preference order specified by the MathML spec.
    If that's not available either, returns the default operator spec.
*/
static const QwtMmlOperSpec *mmlFindOperSpec( const QString &text,
                                              QwtMml::FormType form )
{
    QStringList name_list;
    name_list.append( text );

    // First, just try to find text in the operator dictionary.
    OperSpecSearchResult result = _mmlFindOperSpec( name_list, form );

    if ( !result.haveForm( form ) )
    {
        // Try to find other names for the operator represented by text.

        const QwtMMLEntityTable::Spec *ent = 0;
        for ( ;; )
        {
            ent = mmlEntityTable.search( text, ent );
            if ( ent == 0 )
                break;
            name_list.append( '&' + QString( ent->name ) + ';' );
            ++ent;
        }

        result = _mmlFindOperSpec( name_list, form );
    }

    const QwtMmlOperSpec *spec = result.getForm( form );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( QwtMml::InfixForm );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( QwtMml::PostfixForm );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( QwtMml::PrefixForm );
    if ( spec != 0 )
        return spec;

    return &g_oper_spec_defaults;
}

static QString mmlDictAttribute( const QString &name, const QwtMmlOperSpec *spec )
{
    int i = attributeIndex( name );
    if ( i == -1 )
        return QString();
    else
        return spec->attributes[i];
}

static int mmlInterpretMathVariant( const QString &value, bool *ok )
{
    struct MathVariantValue
    {
        QString value;
        int mv;
    };

    static const MathVariantValue g_mv_data[] =
    {
        { "normal",                 QwtMml::NormalMV                                        },
        { "bold",                   QwtMml::BoldMV                                          },
        { "italic",                 QwtMml::ItalicMV                                        },
        { "bold-italic",            QwtMml::BoldMV | QwtMml::ItalicMV                       },
        { "double-struck",          QwtMml::DoubleStruckMV                                  },
        { "bold-fraktur",           QwtMml::BoldMV | QwtMml::FrakturMV                      },
        { "script",                 QwtMml::ScriptMV                                        },
        { "bold-script",            QwtMml::BoldMV | QwtMml::ScriptMV                       },
        { "fraktur",                QwtMml::FrakturMV                                       },
        { "sans-serif",             QwtMml::SansSerifMV                                     },
        { "bold-sans-serif",        QwtMml::BoldMV | QwtMml::SansSerifMV                    },
        { "sans-serif-italic",      QwtMml::SansSerifMV | QwtMml::ItalicMV                  },
        { "sans-serif-bold-italic", QwtMml::SansSerifMV | QwtMml::ItalicMV | QwtMml::BoldMV },
        { "monospace",              QwtMml::MonospaceMV                                     },
        { 0,                        0                                                       }
    };

    const MathVariantValue *v = g_mv_data;
    for ( ; v->value != 0; ++v )
    {
        if ( value == v->value )
        {
            if ( ok != 0 )
                *ok = true;
            return v->mv;
        }
    }

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretMathVariant(): could not parse value: \"%s\"", qPrintable( value ) );

    return QwtMml::NormalMV;
}

static QwtMml::FormType mmlInterpretForm( const QString &value, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "prefix" )
        return QwtMml::PrefixForm;
    if ( value == "infix" )
        return QwtMml::InfixForm;
    if ( value == "postfix" )
        return QwtMml::PostfixForm;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretForm(): could not parse value \"%s\"", qPrintable( value ) );
    return QwtMml::InfixForm;
}

static QwtMml::ColAlign mmlInterpretColAlign(
    const QString &value_list, int colnum, bool *ok )
{
    QString value = mmlInterpretListAttr( value_list, colnum, "center" );

    if ( ok != 0 )
        *ok = true;

    if ( value == "left" )
        return QwtMml::ColAlignLeft;
    if ( value == "right" )
        return QwtMml::ColAlignRight;
    if ( value == "center" )
        return QwtMml::ColAlignCenter;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretColAlign(): could not parse value \"%s\"", qPrintable( value ) );
    return QwtMml::ColAlignCenter;
}

static QwtMml::RowAlign mmlInterpretRowAlign(
    const QString &value_list, int rownum, bool *ok )
{
    QString value = mmlInterpretListAttr( value_list, rownum, "axis" );

    if ( ok != 0 )
        *ok = true;

    if ( value == "top" )
        return QwtMml::RowAlignTop;
    if ( value == "center" )
        return QwtMml::RowAlignCenter;
    if ( value == "bottom" )
        return QwtMml::RowAlignBottom;
    if ( value == "baseline" )
        return QwtMml::RowAlignBaseline;
    if ( value == "axis" )
        return QwtMml::RowAlignAxis;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretRowAlign(): could not parse value \"%s\"", qPrintable( value ) );
    return QwtMml::RowAlignAxis;
}

static QString mmlInterpretListAttr(
    const QString &value_list, int idx, const QString &def )
{
    QStringList l = value_list.split( ' ' );

    if ( l.count() == 0 )
        return def;

    if ( l.count() <= idx )
        return l[l.count() - 1];
    else
        return l[idx];
}

static QwtMml::FrameType mmlInterpretFrameType(
    const QString &value_list, int idx, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    QString value = mmlInterpretListAttr( value_list, idx, "none" );

    if ( value == "none" )
        return QwtMml::FrameNone;
    if ( value == "solid" )
        return QwtMml::FrameSolid;
    if ( value == "dashed" )
        return QwtMml::FrameDashed;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretFrameType(): could not parse value \"%s\"", qPrintable( value ) );
    return QwtMml::FrameNone;
}

static QwtMml::FrameSpacing mmlInterpretFrameSpacing(
    const QString &value_list, qreal em, qreal ex, bool *ok )
{
    QwtMml::FrameSpacing fs;

    QStringList l = value_list.split( ' ' );
    if ( l.count() != 2 )
    {
        qWarning( "interpretFrameSpacing: could not parse value \"%s\"", qPrintable( value_list ) );
        if ( ok != 0 )
            *ok = false;
        return QwtMml::FrameSpacing( 0.4 * em, 0.5 * ex );
    }

    bool hor_ok, ver_ok;
    fs.m_hor = mmlInterpretSpacing( l[0], em, ex, &hor_ok );
    fs.m_ver = mmlInterpretSpacing( l[1], em, ex, &ver_ok );

    if ( ok != 0 )
        *ok = hor_ok && ver_ok;

    return fs;
}

static QFont mmlInterpretDepreciatedFontAttr(
    const QwtMmlAttributeMap &font_attr, QFont &fn, qreal em, qreal ex )
{
    if ( font_attr.contains( "fontsize" ) )
    {
        QString value = font_attr["fontsize"];

        for ( ;; )
        {
            bool ok;
            qreal ptsize = mmlInterpretPointSize( value, &ok );
            if ( ok )
            {
                fn.setPointSizeF( ptsize );
                break;
            }

            ptsize = mmlInterpretPercentSpacing( value, fn.pointSize(), &ok );
            if ( ok )
            {
                fn.setPointSizeF( ptsize );
                break;
            }

            qreal size = mmlInterpretSpacing( value, em, ex, &ok );
            if ( ok )
            {
#if 1
                fn.setPixelSize( size );
#endif
                break;
            }

            break;
        }
    }

    if ( font_attr.contains( "fontweight" ) )
    {
        QString value = font_attr["fontweight"];
        if ( value == "normal" )
            fn.setBold( false );
        else if ( value == "bold" )
            fn.setBold( true );
        else
            qWarning( "interpretDepreciatedFontAttr(): could not parse fontweight \"%s\"", qPrintable( value ) );
    }

    if ( font_attr.contains( "fontstyle" ) )
    {
        QString value = font_attr["fontstyle"];
        if ( value == "normal" )
            fn.setItalic( false );
        else if ( value == "italic" )
            fn.setItalic( true );
        else
            qWarning( "interpretDepreciatedFontAttr(): could not parse fontstyle \"%s\"", qPrintable( value ) );
    }

    if ( font_attr.contains( "fontfamily" ) )
    {
        QString value = font_attr["fontfamily"];
        fn.setFamily( value );
    }

    return fn;
}

static QFont mmlInterpretMathSize( const QString &value, QFont &fn, qreal em, qreal ex, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "small" )
    {
        fn.setPointSizeF( fn.pointSizeF() * 0.7 );
        return fn;
    }

    if ( value == "normal" )
        return fn;

    if ( value == "big" )
    {
        fn.setPointSizeF( fn.pointSizeF() * 1.5 );
        return fn;
    }

    bool size_ok;

    qreal ptsize = mmlInterpretPointSize( value, &size_ok );
    if ( size_ok )
    {
        fn.setPointSizeF( ptsize );
        return fn;
    }

    qreal size = mmlInterpretSpacing( value, em, ex, &size_ok );
    if ( size_ok )
    {
#if 1
        fn.setPixelSize( size );  // setPointSizeF ???
#endif
        return fn;
    }

    if ( ok != 0 )
        *ok = false;
    qWarning( "interpretMathSize(): could not parse mathsize \"%s\"", qPrintable( value ) );
    return fn;
}

/*!
    \class QwtMathMLDocument

    \brief The QwtMathMLDocument class renders mathematical formulas written in MathML 2.0.
*/

/*!
  Constructs an empty MML document.
*/
QwtMathMLDocument::QwtMathMLDocument()
{
    m_doc = new QwtMmlDocument;
}

/*!
  Destroys the MML document.
*/
QwtMathMLDocument::~QwtMathMLDocument()
{
    delete m_doc;
}

/*!
    Clears the contents of this MML document.
*/
void QwtMathMLDocument::clear()
{
    m_doc->clear();
}

/*!
    Sets the MathML expression to be rendered. The expression is given
    in the string \a text. If the expression is successfully parsed,
    this method returns true; otherwise it returns false. If an error
    occured \a errorMsg is set to a diagnostic message, while \a
    errorLine and \a errorColumn contain the location of the error.
    Any of \a errorMsg, \a errorLine and \a errorColumn may be 0,
    in which case they are not set.

    \a text should contain MathML 2.0 presentation markup elements enclosed
    in a <math> element.
*/
bool QwtMathMLDocument::setContent( const QString &text, QString *errorMsg,
                                    int *errorLine, int *errorColumn )
{
    return m_doc->setContent( text, errorMsg, errorLine, errorColumn );
}

/*!
  Renders this MML document with the painter \a painter at position \a pos.
*/
void QwtMathMLDocument::paint( QPainter *painter, const QPointF &pos ) const
{
    m_doc->paint( painter, pos );
}

/*!
    Returns the size of this MML document, as rendered, in pixels.
*/
QSizeF QwtMathMLDocument::size() const
{
    return m_doc->size();
}

/*!
    Returns the name of the font used to render the font \a type.

    \sa setFontName()  setBaseFontPointSize() baseFontPointSize() QwtMathMLDocument::MmlFont
*/
QString QwtMathMLDocument::fontName( QwtMathMLDocument::MmlFont type ) const
{
    return m_doc->fontName( type );
}

/*!
    Sets the name of the font used to render the font \a type to \a name.

    \sa fontName() setBaseFontPointSize() baseFontPointSize() QwtMathMLDocument::MmlFont
*/
void QwtMathMLDocument::setFontName( QwtMathMLDocument::MmlFont type,
                                     const QString &name )
{
    if ( name == m_doc->fontName( type ) )
        return;

    m_doc->setFontName( type, name );
    m_doc->layout();
}

/*!
    Returns the point size of the font used to render expressions
    which scriptlevel is 0.

    \sa setBaseFontPointSize() fontName() setFontName()
*/
qreal QwtMathMLDocument::baseFontPointSize() const
{
    return m_doc->baseFontPointSize();
}

/*!
    Sets the point \a size of the font used to render expressions
    which scriptlevel is 0.

    \sa baseFontPointSize() fontName() setFontName()
*/
void QwtMathMLDocument::setBaseFontPointSize( qreal size )
{
    if ( size < g_min_font_point_size )
        size = g_min_font_point_size;

    if ( size == m_doc->baseFontPointSize() )
        return;

    m_doc->setBaseFontPointSize( size );
    m_doc->layout();
}

/*!
    Returns the color used to render expressions.
*/
QColor QwtMathMLDocument::foregroundColor() const
{
    return m_doc->foregroundColor();
}

/*!
    Sets the color used to render expressions.
*/
void QwtMathMLDocument::setForegroundColor( const QColor &color )
{
    if ( color == m_doc->foregroundColor() )
        return;

    m_doc->setForegroundColor( color );
}

/*!
    Returns the color used to render the background of expressions.
*/
QColor QwtMathMLDocument::backgroundColor() const
{
    return m_doc->backgroundColor();
}

/*!
    Sets the color used to render the background of expressions.
*/
void QwtMathMLDocument::setBackgroundColor( const QColor &color )
{
    if ( color == m_doc->backgroundColor() )
        return;

    m_doc->setBackgroundColor( color );
}

#ifdef MML_TEST
/*!
    Returns whether frames are to be drawn.
*/
bool QwtMathMLDocument::drawFrames() const
{
    return m_doc->drawFrames();
}

/*!
    Specifies whether frames are to be drawn.
*/
void QwtMathMLDocument::setDrawFrames( bool drawFrames )
{
    if ( drawFrames == m_doc->drawFrames() )
        return;

    m_doc->setDrawFrames( drawFrames );
}

#endif
