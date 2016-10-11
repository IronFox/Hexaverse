<?php
require_once('visual/site.php');
require_once('renderer/WikiRenderer.lib.php');
require_once(WIKIRENDERER_PATH . 'rules/dokuwiki_to_xhtml.php');

function BeginsWith($strHaystack, $strNeedle)
{
	return substr($strHaystack,0,strlen($strNeedle)) == $strNeedle;
}

function GetTagValue($strHaystack, $tag)
{
	if (!BeginsWith($strHaystack, $tag.':'))
		return null;
	return substr($strHaystack,strlen($tag)+1);
}

class MyConfig extends dokuwiki_to_xhtml
{
	/**
	* in some wiki system, some links are specials. You should override this method
	* to transform this specific links to real URL
	* @return array  first item is the url, second item is an alternate label
	*/
	public function processLink($url, $tagName='')
	{
		$rs = parent::processLink($url, $tagName);
		$url = $rs[0];
		if ($appendix = GetTagValue($url,'img'))
			return array("./images/$appendix", $rs[1]);
		return array("./html.php?page=$url", $rs[1]);
	}
};

$ctr=new WikiRenderer(new MyConfig());

function Convert($strPage)
{
	global $ctr;
	$text = implode('',file("./documentation/$strPage.txt"));
	$text = $ctr->render($text);
	
	$rs = array();
	
	if($ctr->errors)
	{
		//echo '<p style="color:red;">Errors in line(s):';
		foreach ($ctr->errors as $line => $l)
			$rs[] = "$line: $l";
	}
	$rs[] = $text;
	return $rs;
}


Site(Convert('home'),"Documentation");