# The following has been generated automatically from src/core/layout/qgslayoutpagecollection.h
try:
    QgsLayoutPageCollection.__attribute_docs__ = {'changed': 'Emitted when pages are added or removed from the collection.\n', 'pageAboutToBeRemoved': 'Emitted just before a page is removed from the collection.\n\nPage numbers in collections begin at 0 - so a page number of 0 indicates\nthe first page.\n'}
    QgsLayoutPageCollection.__overridden_methods__ = ['stringType', 'layout', 'writeXml', 'readXml']
    QgsLayoutPageCollection.__signal_arguments__ = {'pageAboutToBeRemoved': ['pageNumber: int']}
    QgsLayoutPageCollection.__group__ = ['layout']
except (NameError, AttributeError):
    pass
